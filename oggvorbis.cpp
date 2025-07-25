#include "oggvorbis.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <inttypes.h>

byte ilog(int64_t i)
{
    byte ret = 0;
    while (i > 0) {
        ret++;
        i >>= 1;
    }
    return ret;
}

const char* str_of_err(err e)
{
    switch (e)
    {
    case OK: return "OK";
    case READ_ERROR: return "Read error (probably EOF)";
    case NO_CAPTURE_PATTERN: return "Could not find a capture pattern (OggS)";
    case PACKET_TOO_LARGE: return "Encountered a packet that was too large";
    case MALLOC: return "Out of memory or other malloc failure";
    case NOT_VORBIS: return "Codec in the ogg stream did not identify as vorbis";
    case INVALID_DATA: return "Invalid data was detected";
    case INVALID_VERSION: return "Invalid vorbis version";
    case INVALID_CHANNELS: return "Invalid number of audio channels";
    case INVALID_SAMPLE_RATE: return "Invalid sample rate";
    case INVALID_BLOCKSIZE_0: return "Invalid blocksize 0";
    case INVALID_BLOCKSIZE_1: return "Invalid blocksize 1";
    case INVALID_CODEBOOK: return "Invalid codebook format";
    case INVALID_MODE: return "Invalid mode";
    case INVALID_MAPPING: return "Invalid mapping";
    case INVALID_FLOOR: return "Invalid floor";
    case FRAMING_ERROR: return "Framing error";
    case INVALID_RESIDUES: return "Invalid residues";
    }
    return "Error handling meta-error: invalid error code";
}

err page_header_read(vorbis_state* s, ogg_page_hdr* hdr)
{
    void* fi = s->datasource;
    hdr->start_pos = s->callbacks.tell_func(fi);
    hdr->capture_pattern[0] = 0;
    if (s->callbacks.read_func(hdr->capture_pattern, 4, 1, fi) != 1)
        return READ_ERROR;
    if (hdr->capture_pattern[0] != 'O'
        || hdr->capture_pattern[1] != 'g'
        || hdr->capture_pattern[2] != 'g'
        || hdr->capture_pattern[3] != 'S') {
        return NO_CAPTURE_PATTERN;
    }
    if (s->callbacks.read_func(&hdr->stream_structure_version, 1, 1, fi) != 1)
        return READ_ERROR;
    if (s->callbacks.read_func(&hdr->header_type_flag, 1, 1, fi) != 1)
        return READ_ERROR;
    if (s->callbacks.read_func(&hdr->granule_pos, 8, 1, fi) != 1)
        return READ_ERROR;
    if (s->callbacks.read_func(&hdr->serial, 4, 1, fi) != 1)
        return READ_ERROR;
    if (s->callbacks.read_func(&hdr->seq_no, 4, 1, fi) != 1)
        return READ_ERROR;
    if (s->callbacks.read_func(&hdr->checksum, 4, 1, fi) != 1)
        return READ_ERROR;
    if (s->callbacks.read_func(&hdr->page_segments, 1, 1, fi) != 1)
        return READ_ERROR;
    if (s->callbacks.read_func(hdr->segment_table, 1, hdr->page_segments, fi) != hdr->page_segments)
        return READ_ERROR;
    return OK;
}

void page_header_print(ogg_page_hdr* hdr)
{
    printf("Capture pattern: %c%c%c%c\n", hdr->capture_pattern[0], hdr->capture_pattern[1], hdr->capture_pattern[2], hdr->capture_pattern[3]);
    printf("Stream structure version: %d\n", hdr->stream_structure_version);
    printf("Header type flag: %x\n", hdr->header_type_flag);
    printf("Granule pos: %" PRId64 "\n", hdr->granule_pos);
    printf("Serial: %d\n", hdr->serial);
    printf("Sequence number: %d\n", hdr->seq_no);
    printf("Checksum: %x\n", hdr->checksum);
    printf("Page segments: %d\n", hdr->page_segments);
    for (int i = 0; i < hdr->page_segments; i++) {
        printf(" Segment %d: %d\n", i, hdr->segment_table[i]);
    }
}

uint64_t vorbis_read_bits(vorbis_packet* s, size_t count, bool d = false)
{
    if (count > 64 || count == 0) {
        return 0;
    }
    size_t bc = s->bitCursor;
    uint64_t ret = 0;
    uint64_t cur = 0;
    while (count > cur)
    {
        size_t idx = bc >> 3;
        size_t bit = bc & 0x7;

        ret |= ((s->buf[idx] >> bit) & 1ull) << cur;

        bc++;
        cur++;
    }
    s->bitCursor = bc;
    if (s->bitCursor > (s->size << 3))
    {
        printf("Warning: read beyond end of packet\n");
    }
    return ret;
}

err vorbis_read_page(vorbis_state* s)
{
    err e;
    s->cur_page_start = s->callbacks.tell_func(s->datasource);
    if ((e = page_header_read(s, &s->cur_page)) != 0)
        return e;
    s->file_pos = s->callbacks.tell_func(s->datasource);
    s->next_segment = 0;
    return OK;
}

err vorbis_read_packet(vorbis_state* s)
{
    err e;
    size_t packet_size = 0;
    size_t packet_read = 0;
    byte segment_length;
    do
    {
        if (s->next_segment >= s->cur_page.page_segments)
        {
            if (packet_size - packet_read > 0)
            {
                s->callbacks.read_func(s->cur_packet.buf + packet_read, 1, packet_size - packet_read, s->datasource);
                packet_read = packet_size;
            }
            if ((e = vorbis_read_page(s)) != OK)
                return e;
        }
        segment_length = s->cur_page.segment_table[s->next_segment++];
        if (packet_size == 0) {
            s->cur_packet_start = s->file_pos;
        }
        packet_size += segment_length;
        s->file_pos += segment_length;
        if (packet_size > MAX_PACKET_SIZE)
        {
            return PACKET_TOO_LARGE;
        }
    } while (segment_length == 255);
    if (packet_size - packet_read > 0)
    {
        s->callbacks.read_func(s->cur_packet.buf + packet_read, 1, packet_size - packet_read, s->datasource);
    }

    s->cur_packet.size = packet_size;
    s->cur_packet.bitCursor = 0;
    return OK;
}

void vorbis_free(vorbis_state* s)
{
    if (s == nullptr) return;
    if (s->cur_packet.buf) {
        free(s->cur_packet.buf);
    }
    free(s);
}

const uint64_t VORBIS_ID = 0x736962726f76ull;

err vorbis_read_id(vorbis_state* s)
{
    err e;
    if ((e = vorbis_read_packet(s)) != OK)
        return e;
    if (s->cur_packet.size != 30)
        return NOT_VORBIS;
    vorbis_packet* p = &s->cur_packet;
    if (vorbis_read_bits(p, 8) != 1)
        return NOT_VORBIS;
    uint64_t v;
    if ((v = vorbis_read_bits(p, 48)) != VORBIS_ID)
    {
        return NOT_VORBIS;
    }
    s->id.vorbis_version = vorbis_read_bits(p, 32);
    s->id.audio_channels = vorbis_read_bits(p, 8);
    s->id.audio_sample_rate = vorbis_read_bits(p, 32);
    s->id.bitrate_maximum = vorbis_read_bits(p, 32);
    s->id.bitrate_nominal = vorbis_read_bits(p, 32);
    s->id.bitrate_minimum = vorbis_read_bits(p, 32);
    s->id.blocksize_0 = vorbis_read_bits(p, 4);
    s->id.blocksize_1 = vorbis_read_bits(p, 4);
    s->id.framing_flag = vorbis_read_bits(p, 1);

    if (s->id.vorbis_version != 0)
        return INVALID_VERSION;
    if (s->id.audio_channels == 0)
        return INVALID_CHANNELS;
    if (s->id.audio_sample_rate == 0)
        return INVALID_SAMPLE_RATE;
    if (s->id.blocksize_0 < 6 || s->id.blocksize_0 > 13)
        return INVALID_BLOCKSIZE_0;
    if (s->id.blocksize_1 < 6 || s->id.blocksize_1 > 13)
        return INVALID_BLOCKSIZE_1;
    if (s->id.blocksize_0 > s->id.blocksize_1)
        return INVALID_BLOCKSIZE_0;
    return OK;
}

err vorbis_read_setup(vorbis_state *s)
{
    err e;
    if ((e = vorbis_read_packet(s)) != OK)
        return e;
    vorbis_packet* p = &s->cur_packet;
    if (vorbis_read_bits(p, 8) != 5)
    {
        return INVALID_DATA;
    }
    if (vorbis_read_bits(p, 48) != VORBIS_ID)
    {
        return INVALID_DATA;
    }

    s->setup.codebook_count = vorbis_read_bits(p, 8) + 1;
    for (auto i = 0; i < s->setup.codebook_count; i++)
    {
        vorbis_codebook *c = &s->setup.codebooks[i];
        if (vorbis_read_bits(p, 24) != 0x564342)
            return INVALID_CODEBOOK;

        uint32_t codebook_dimensions = static_cast<uint32_t>(vorbis_read_bits(p, 16, true));
        uint32_t codebook_entries = static_cast<uint32_t>(vorbis_read_bits(p, 24, true));
        c->entries = static_cast<byte*>(malloc(codebook_entries));
        if (!c->entries)
            return MALLOC;
        if (!vorbis_read_bits(p, 1))
        {
            bool sparse = vorbis_read_bits(p, 1);
            for (int j = 0; j < codebook_entries; j++)
            {
                if (sparse)
                {
                    if (vorbis_read_bits(p, 1))
                    {
                        c->entries[j] = vorbis_read_bits(p, 5) + 1;
                    }
                    else {
                        c->entries[j] = 0;
                    }
                }
                else
                {
                    c->entries[j] = vorbis_read_bits(p, 5) + 1;
                }
            }
        }
        else
        {
            byte length = vorbis_read_bits(p, 5) + 1;
            for (int j = 0; j != codebook_entries; length++)
            {
                byte number = vorbis_read_bits(p, ilog(codebook_entries - j));
                for (int k = j; k < (j + number); k++)
                    c->entries[k] = length;
                j += number;
                if (j > codebook_entries)
                    return INVALID_CODEBOOK;
            }
        }

        c->lookup_type = vorbis_read_bits(p, 4);
        if (c->lookup_type > 2)
            return INVALID_CODEBOOK;
        else if (c->lookup_type > 0)
        {
            vorbis_read_bits(p, 32);
            vorbis_read_bits(p, 32);
            byte value_bits = vorbis_read_bits(p, 4) + 1;
            vorbis_read_bits(p, 1);
            uint64_t lookup_values = 0;
            if (c->lookup_type == 1)
            {
                while (std::pow(static_cast<double>(lookup_values), static_cast<double>(codebook_dimensions)) <= codebook_entries)
                {
                    lookup_values++;
                }
                if (lookup_values > 0)
                    lookup_values--;

            }
            else
            {
                lookup_values = codebook_entries * codebook_dimensions;
            }

            for (uint64_t j = 0; j < lookup_values; j++)
            {
                vorbis_read_bits(p, value_bits);
            }
        }
    }

    auto time_count = vorbis_read_bits(p, 6) + 1;
    for (auto i = 0ull; i < time_count; i++)
    {
        if (vorbis_read_bits(p, 16) != 0)
            return INVALID_DATA;
    }

    s->setup.floor_count = vorbis_read_bits(p, 6) + 1;
    for (auto i = 0; i < s->setup.floor_count; i++)
    {
        uint16_t floor_type = vorbis_read_bits(p, 16);
        if (floor_type == 0)
        {
            vorbis_read_bits(p, 8);
            vorbis_read_bits(p, 16);
            vorbis_read_bits(p, 16);
            vorbis_read_bits(p, 6);
            vorbis_read_bits(p, 8);
            byte number_of_books = vorbis_read_bits(p, 4) + 1;
            for (int i = 0; i < number_of_books; i++)
            {
                vorbis_read_bits(p, 8);
            }
        }
        else if (floor_type == 1)
        {
            byte class_list[32];
            byte dimensions[16];
            byte subclasses[16];
            byte masterbooks[16];

            byte partitions = vorbis_read_bits(p, 5);
            int max_class = -1;
            for (auto j = 0; j < partitions; j++)
            {
                class_list[j] = vorbis_read_bits(p, 4);
                if (class_list[j] > max_class)
                    max_class = class_list[j];
            } 
            for (auto j = 0; j <= max_class; j++)
            {
                dimensions[j] = vorbis_read_bits(p, 3) + 1;
                subclasses[j] = vorbis_read_bits(p, 2);
                if (subclasses[j])
                {
                    masterbooks[j] = vorbis_read_bits(p, 8);
                    if (masterbooks[j] >= s->setup.codebook_count)
                        return INVALID_FLOOR;
                }
                int exp = 1 << subclasses[j];

                for (auto k = 0; k < exp; k++)
                {
                    int16_t subclass_books = static_cast<int16_t>(vorbis_read_bits(p, 8)) - 1;
                    if (subclass_books >= s->setup.codebook_count)
                        return INVALID_FLOOR;
                }
            }
            
            vorbis_read_bits(p, 2);
            byte rangebits = vorbis_read_bits(p, 4);
            byte floor1_values = 2;
            for (auto j = 0; j < partitions; j++)
            {
                byte classnum = class_list[j];

                for (auto k = 0; k < dimensions[classnum]; k++)
                {
                    vorbis_read_bits(p, rangebits);
                    floor1_values++;
                    if (floor1_values > 64)
                        return INVALID_FLOOR;
                }
            }
        }
        else
        {
            return INVALID_FLOOR;
        }
    }

    s->setup.residue_count = vorbis_read_bits(p, 6) + 1;
    for (auto i = 0; i < s->setup.residue_count; i++)
    {
        vorbis_residue *r = &s->setup.residue_configurations[i];
        uint16_t residue_types = vorbis_read_bits(p, 16);
        if (residue_types > 2)
            return INVALID_RESIDUES;
        r->begin = vorbis_read_bits(p, 24);
        r->end = vorbis_read_bits(p, 24);
        r->partition_size = vorbis_read_bits(p, 24) + 1;
        r->residue_classifications = vorbis_read_bits(p, 6) + 1;
        r->residue_classbook = vorbis_read_bits(p, 8);
        for (int j = 0; j < r->residue_classifications; j++)
        {
            int high_bits = 0;
            int low_bits = vorbis_read_bits(p, 3);
            if (vorbis_read_bits(p, 1))
            {
                high_bits = vorbis_read_bits(p, 5);
            }
            r->residue_cascades[j] = (high_bits << 3) | low_bits;
        }
        for (int j = 0; j < r->residue_classifications; j++)
        {
            for (int k = 0; k < 8; k++)
            {
                if (r->residue_cascades[j] & (1 << k))
                {
                    vorbis_read_bits(p, 8);
                }
            }
        }
    }

    s->setup.mapping_count = vorbis_read_bits(p, 6) + 1;
    for (auto i = 0; i < s->setup.mapping_count; i++)
    {
        vorbis_mapping* m = &s->setup.mapping_configurations[i];
        if (vorbis_read_bits(p, 16) != 0)
            return INVALID_MAPPING;
        if (vorbis_read_bits(p, 1))
        {
            m->submaps = vorbis_read_bits(p, 4) + 1;
        }
        else
        {
            m->submaps = 1;
        }

        if (vorbis_read_bits(p, 1))
        {
            m->coupling_steps = vorbis_read_bits(p, 8) + 1;
            for (int j = 0; j < m->coupling_steps; j++)
            {
                vorbis_read_bits(p, ilog(s->id.audio_channels - 1));
                vorbis_read_bits(p, ilog(s->id.audio_channels - 1));
            }
        }
        else
        {
            m->coupling_steps = 0;
        }

        if (vorbis_read_bits(p, 2) != 0)
            return INVALID_MAPPING;
        if (m->submaps > 1)
        {
            for (int j = 0; j < s->id.audio_channels; j++)
            {
                if (vorbis_read_bits(p, 4) > m->submaps)
                    return INVALID_MAPPING;
            }
        }

        for (int j = 0; j < m->submaps; j++)
        {
            vorbis_read_bits(p, 8);
            if (vorbis_read_bits(p, 8) > s->setup.floor_count)
                return INVALID_MAPPING;
            if (vorbis_read_bits(p, 8) > s->setup.residue_count)
                return INVALID_MAPPING;
        }
    }

    s->setup.mode_count = vorbis_read_bits(p, 6) + 1;
    vorbis_mode* modes = s->setup.mode_configurations;
    for (auto i = 0; i < s->setup.mode_count; i++)
    {
        modes[i].blockflag = vorbis_read_bits(p, 1);
        modes[i].windowtype = vorbis_read_bits(p, 16);
        modes[i].transformtype = vorbis_read_bits(p, 16);
        modes[i].mapping = vorbis_read_bits(p, 8);
        if (modes[i].windowtype != 0
            || modes[i].transformtype != 0
            || modes[i].mapping >= s->setup.mapping_count)
            return INVALID_MODE;
    }
    if (vorbis_read_bits(p, 1) == 0)
        return FRAMING_ERROR;

    return OK;
}

err vorbis_init(void* datasource, vorbis_state **out, ov_callbacks callbacks)
{
    err e;
    vorbis_state *s = static_cast<vorbis_state*>(calloc(sizeof(vorbis_state), 1));
    if (!s) {
        e = MALLOC;
        goto fail;
    }
    s->callbacks = callbacks;
    s->datasource = datasource;
    s->next_sample = 0;
    s->file_pos = 0;
    s->last_bs = 0;
    s->cur_packet.buf = static_cast<byte*>(malloc(MAX_PACKET_SIZE));
    if (!s->cur_packet.buf)
    {
        e = MALLOC;
        goto fail;
    }
    s->cur_packet.size = 0;
    if ((e = vorbis_read_page(s)) != OK)
        goto fail;

    if ((e = vorbis_read_id(s)) != OK)
        goto fail;

    if ((e = vorbis_read_packet(s)) != OK)
        goto fail;
    if (vorbis_read_bits(&s->cur_packet, 8) != 3)
    {
        e = INVALID_DATA;
        goto fail;
    }

    if ((e = vorbis_read_setup(s)) != OK)
        goto fail;

    *out = s;
    return OK;

fail:
    vorbis_free(s);
    *out = nullptr;
    return e;
}

err vorbis_next(vorbis_state* vb)
{
    err e;
    if ((e = vorbis_read_packet(vb)) != OK)
        return e;
    vorbis_packet *p = &vb->cur_packet;

    if (vorbis_read_bits(p, 1) != 0)
        return INVALID_DATA;
    uint32_t mode_number = vorbis_read_bits(p, ilog(vb->setup.mode_count - 1));
    uint32_t blocksize = vb->setup.mode_configurations[mode_number].blockflag
        ? vb->id.blocksize_1 : vb->id.blocksize_0;
    blocksize = 1 << blocksize;
    if (vb->last_bs)
        vb->next_sample += (vb->last_bs + blocksize) / 4;
    vb->last_bs = blocksize;
    
    return OK;
}
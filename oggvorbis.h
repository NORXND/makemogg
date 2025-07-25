#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdbool> // for bool

#ifndef _OV_FILE_H_
#include "XiphTypes.h"
#endif

// This is a barebones implementation of Ogg Vorbis based on the specs at xiph.org
// This doesn't decode PCM samples. It only reads vorbis packets for their blocksize.

enum err {
    OK = 0,
    READ_ERROR,
    NO_CAPTURE_PATTERN,
    PACKET_TOO_LARGE,
    MALLOC,
    NOT_VORBIS,
    INVALID_DATA,
    INVALID_VERSION,
    INVALID_CHANNELS,
    INVALID_SAMPLE_RATE,
    INVALID_BLOCKSIZE_0,
    INVALID_BLOCKSIZE_1,
    INVALID_CODEBOOK,
    INVALID_MODE,
    INVALID_MAPPING,
    INVALID_FLOOR,
    INVALID_RESIDUES,
    FRAMING_ERROR,
};

typedef uint8_t byte;

constexpr size_t MAX_PACKET_SIZE = 0x8000; // Don't deal with packets over this size (32k)

struct ogg_page_hdr {
    char capture_pattern[4];
    byte stream_structure_version;
    byte header_type_flag;
    int64_t granule_pos;
    int32_t serial;
    int32_t seq_no;
    int32_t checksum;
    byte page_segments;
    byte segment_table[256];
    long start_pos;
};

struct vorbis_packet {
    byte* buf;
    size_t bitCursor;
    size_t size;
};

struct vorbis_id_header {
    uint32_t vorbis_version;
    uint8_t audio_channels;
    uint32_t audio_sample_rate;
    int32_t bitrate_maximum;
    int32_t bitrate_nominal;
    int32_t bitrate_minimum;
    uint8_t blocksize_0;
    uint8_t blocksize_1;
    uint8_t framing_flag;
};

struct vorbis_codebook {
    uint32_t entry_count;
    byte* entries;
    uint8_t lookup_type;
};

struct vorbis_residue {
    uint32_t begin;
    uint32_t end;
    uint32_t partition_size;
    uint8_t residue_classifications;
    uint8_t residue_classbook;
    uint8_t residue_cascades[64];
};

struct vorbis_mapping {
    uint8_t submaps;
    uint16_t coupling_steps;
};

struct vorbis_mode {
    bool blockflag;
    uint16_t windowtype;
    uint16_t transformtype;
    uint8_t mapping;
};

struct vorbis_setup_header {
    uint8_t codebook_count;
    vorbis_codebook codebooks[256];
    // time_count
    uint8_t floor_count;
    uint16_t floor_types[64];
    // If you use vorbis_floor, define it appropriately
    // vorbis_floor floor_configurations[64];

    uint8_t residue_count;
    uint16_t residue_types[64];
    vorbis_residue residue_configurations[64];

    uint8_t mapping_count;
    vorbis_mapping mapping_configurations[64];

    uint8_t mode_count;
    vorbis_mode mode_configurations[64];
};

struct vorbis_state {
    ov_callbacks callbacks;
    void* datasource;
    size_t file_pos;
    ogg_page_hdr cur_page;
    size_t cur_page_start;
    size_t next_segment;
    vorbis_packet cur_packet;
    int64_t next_sample;
    uint32_t last_bs;
    size_t cur_packet_start;
    vorbis_id_header id;
    vorbis_setup_header setup;
};

// API
const char* str_of_err(err e);
err vorbis_init(void* datasource, vorbis_state **out, ov_callbacks callbacks);
void vorbis_free(vorbis_state* s);
err vorbis_next(vorbis_state* s);
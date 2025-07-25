#ifndef MAKEMOGG_LIB_H
#define MAKEMOGG_LIB_H

#ifdef _WIN32
#  ifdef MAKEMOGG_EXPORTS
#    define MAKEMOGG_API __declspec(dllexport)
#  else
#    define MAKEMOGG_API __declspec(dllimport)
#  endif
#else
#  define MAKEMOGG_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Create an unencrypted mogg file from input ogg
// Returns 0 on success, nonzero on error
MAKEMOGG_API int makemogg_create_unencrypted(const char* input_path, const char* output_path);

// Example API: process a mogg file (dummy, for compatibility)
// Returns 0 on success, nonzero on error
MAKEMOGG_API int makemogg_process(const char* input_path, const char* output_path);

#ifdef __cplusplus
}
#endif

#endif // MAKEMOGG_LIB_H

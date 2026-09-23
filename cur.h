#ifndef CUR_H
#define CUR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(CURLIB_BUILD)
#define CURLIB_API __declspec(dllexport)
#elif defined(_WIN32) && !defined(CURLIB_STATIC)
#define CURLIB_API __declspec(dllimport)
#else
#define CURLIB_API
#endif

typedef struct {
    char device_gbk[81];
    char name_gbk[21];
    float *values;
} CurCurve;

typedef struct {
    uint32_t header0;
    uint32_t curve_count;
    uint32_t sample_count;
    CurCurve *curves;
} CurFile;

CURLIB_API int cur_read(const char *path, CurFile *out, char *error, size_t error_size);
CURLIB_API int cur_write(const char *path, const CurFile *file, char *error, size_t error_size);
CURLIB_API int cur_validate(const char *path, char *error, size_t error_size);
CURLIB_API int cur_export_swx(const char *path, const CurFile *file, char *error, size_t error_size);
CURLIB_API void cur_free(CurFile *file);

#ifdef __cplusplus
}
#endif
#endif

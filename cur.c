#include "cur.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <wchar.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* 将 UTF-8 路径转换为当前平台可用的二进制文件流。 */
static FILE *open_binary(const char *path, const wchar_t *wide_mode, const char *mode) {
#ifdef _WIN32
    int length;
    wchar_t *wide_path;
    FILE *fp;
    if (!path) return NULL;
    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (length <= 0) return NULL;
    wide_path = (wchar_t *)malloc((size_t)length * sizeof(wchar_t));
    if (!wide_path) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide_path, length)) {
        free(wide_path);
        return NULL;
    }
    fp = _wfopen(wide_path, wide_mode);
    free(wide_path);
    return fp;
#else
    (void)wide_mode;
    return path ? fopen(path, mode) : NULL;
#endif
}

static void set_error(char *dst, size_t n, const char *fmt, ...) {
    if (!dst || !n) return;
    va_list ap; va_start(ap, fmt); vsnprintf(dst, n, fmt, ap); va_end(ap);
}

static size_t bounded_len(const char *s, size_t max) {
    size_t n = 0;
    while (n < max && s[n]) ++n;
    return n;
}

/* 检查二进制头部是否包含指定的 ASCII 标记。 */
static int contains_ascii(const unsigned char *data, size_t size, const char *text) {
    size_t i, n = strlen(text);
    if (!n || n > size) return 0;
    for (i = 0; i + n <= size; ++i) {
        if (!memcmp(data + i, text, n)) return 1;
    }
    return 0;
}

static uint32_t u32le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put_u32le(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

static int read_all(const char *path, unsigned char **data, size_t *size, char *err, size_t errn) {
    FILE *fp = open_binary(path, L"rb", "rb");
    long length;
    if (!fp) { set_error(err, errn, "无法打开文件: %s", path); return 0; }
    if (fseek(fp, 0, SEEK_END) || (length = ftell(fp)) < 16 || fseek(fp, 0, SEEK_SET)) {
        fclose(fp); set_error(err, errn, "无法读取文件大小"); return 0;
    }
    *size = (size_t)length; *data = (unsigned char *)malloc(*size);
    if (!*data || fread(*data, 1, *size, fp) != *size) {
        free(*data); *data = NULL; fclose(fp); set_error(err, errn, "读取文件失败"); return 0;
    }
    fclose(fp); return 1;
}

void cur_free(CurFile *file) {
    uint32_t i;
    if (!file) return;
    if (file->curves) {
        for (i = 0; i < file->curve_count; ++i) free(file->curves[i].values);
    }
    free(file->curves);
    memset(file, 0, sizeof(*file));
}

int cur_read(const char *path, CurFile *out, char *err, size_t errn) {
    unsigned char *raw = NULL; size_t size, offset, required, data_size, payload_size; uint32_t i, j;
    int fds;
    if (!out) { set_error(err, errn, "输出参数为空"); return 0; }
    memset(out, 0, sizeof(*out));
    if (!read_all(path, &raw, &size, err, errn)) return 0;
    out->header0 = u32le(raw);
    out->curve_count = u32le(raw + 4) + 1;
    out->sample_count = u32le(raw + 8);
    if (!out->curve_count || !out->sample_count || out->curve_count > 10000 || out->sample_count > 100000000) {
        set_error(err, errn, "文件头中的曲线数或采样点数无效"); free(raw); return 0;
    }
    fds = contains_ascii(raw, size < 316 ? size : 316, "Executed Time") &&
          contains_ascii(raw, size < 316 ? size : 316, "PowerFlow FileName");
    offset = fds ? 16 + ((size_t)out->curve_count + 3) * 100
                 : 16 + (size_t)(out->curve_count - 1) * 100;
    required = offset + (size_t)out->curve_count * out->sample_count * 4;
    if ((size_t)out->sample_count > SIZE_MAX / 4 / out->curve_count) {
        set_error(err, errn, "文件长度超出平台限制");
        free(raw); return 0;
    }
    data_size = (size_t)out->curve_count * out->sample_count * 4;
    if (offset > SIZE_MAX - data_size) {
        set_error(err, errn, "文件长度超出平台限制");
        free(raw); return 0;
    }
    required = offset + data_size;
    payload_size = size >= offset ? size - offset : 0;
    if (payload_size && raw[size - 1] == 0x1A) --payload_size;
    if ((!fds && payload_size < data_size) ||
        (fds && (payload_size > data_size || data_size - payload_size != 80))) {
        set_error(err, errn, "文件长度不足，需要 %zu 字节，实际 %zu 字节", required, size);
        free(raw); return 0;
    }
    out->curves = (CurCurve *)calloc(out->curve_count, sizeof(CurCurve));
    if (!out->curves) { set_error(err, errn, "内存分配失败"); free(raw); return 0; }
    for (i = fds ? 0 : 1; i < out->curve_count; ++i) {
        size_t h = fds ? 16 + ((size_t)i + 3) * 100
                       : 16 + (size_t)(i - 1) * 100;
        if (fds) {
            memcpy(out->curves[i].device_gbk, raw + h, 20);
            memcpy(out->curves[i].name_gbk, raw + h + 20, 20);
        } else {
            memcpy(out->curves[i].device_gbk, raw + h, 80);
            memcpy(out->curves[i].name_gbk, raw + h + 80, 20);
        }
        out->curves[i].device_gbk[80] = 0; out->curves[i].name_gbk[20] = 0;
    }
    for (i = 0; i < out->curve_count; ++i) {
        out->curves[i].values = (float *)malloc((size_t)out->sample_count * sizeof(float));
        if (!out->curves[i].values) { set_error(err, errn, "内存分配失败"); free(raw); cur_free(out); return 0; }
        for (j = 0; j < out->sample_count; ++j) {
            size_t position = ((size_t)i * out->sample_count + j) * 4;
            uint32_t bits = position + 4 <= payload_size ? u32le(raw + offset + position) : 0;
            memcpy(&out->curves[i].values[j], &bits, sizeof(bits));
        }
    }
    free(raw); return 1;
}

int cur_validate(const char *path, char *err, size_t errn) {
    CurFile file; int ok = cur_read(path, &file, err, errn); if (ok) cur_free(&file); return ok;
}

int cur_write(const char *path, const CurFile *file, char *err, size_t errn) {
    FILE *fp; unsigned char b[4]; uint32_t i, j; size_t n;
    if (!path || !file || !file->curves || !file->curve_count || !file->sample_count ||
        file->curve_count > 10000 || file->sample_count > 100000000 || file->curve_count > UINT32_MAX - 1) {
        set_error(err, errn, "无效的 CUR 数据"); return 0;
    }
    for (i = 0; i < file->curve_count; ++i) {
        if (!file->curves[i].values) { set_error(err, errn, "无效的 CUR 数据"); return 0; }
    }
    fp = open_binary(path, L"wb", "wb"); if (!fp) { set_error(err, errn, "无法创建文件: %s", path); return 0; }
    put_u32le(b, file->header0); if (fwrite(b, 1, 4, fp) != 4) goto fail;
    put_u32le(b, file->curve_count - 1); if (fwrite(b, 1, 4, fp) != 4) goto fail;
    put_u32le(b, file->sample_count); if (fwrite(b, 1, 4, fp) != 4) goto fail;
    put_u32le(b, 0); if (fwrite(b, 1, 4, fp) != 4) goto fail;
    for (i = 1; i < file->curve_count; ++i) {
        n = bounded_len(file->curves[i].device_gbk, 80); if (fwrite(file->curves[i].device_gbk, 1, n, fp) != n) goto fail;
        for (; n < 80; ++n) fputc(' ', fp);
        n = bounded_len(file->curves[i].name_gbk, 20); if (fwrite(file->curves[i].name_gbk, 1, n, fp) != n) goto fail;
        for (; n < 20; ++n) fputc(' ', fp);
    }
    for (i = 0; i < file->curve_count; ++i) for (j = 0; j < file->sample_count; ++j) {
        uint32_t bits; memcpy(&bits, &file->curves[i].values[j], sizeof(bits)); put_u32le(b, bits);
        if (fwrite(b, 1, 4, fp) != 4) goto fail;
    }
    fputc(0x1A, fp); if (fclose(fp)) goto closed_fail; return 1;
fail:
    fclose(fp); set_error(err, errn, "写入文件失败"); return 0;
closed_fail:
    set_error(err, errn, "关闭文件失败"); return 0;
}

int cur_export_swx(const char *path, const CurFile *file, char *err, size_t errn) {
    FILE *fp; uint32_t i, j;
    if (!path || !file || !file->curves || !file->curve_count || !file->sample_count ||
        file->curve_count > 10000 || file->sample_count > 100000000) {
        set_error(err, errn, "无效的 CUR 数据"); return 0;
    }
    for (i = 0; i < file->curve_count; ++i) {
        if (!file->curves[i].values) { set_error(err, errn, "无效的 CUR 数据"); return 0; }
    }
    fp = open_binary(path, L"wb", "wb"); if (!fp) { set_error(err, errn, "无法创建 SWX 文件"); return 0; }
    fputs("// CUR export\r\n// columns follow the original CUR order\r\n", fp);
    for (i = 0; i < file->curve_count; ++i) {
        if (i) fputc('\t', fp);
        if (i == 0) fputs("时间", fp); else fwrite(file->curves[i].name_gbk, 1, bounded_len(file->curves[i].name_gbk, 20), fp);
    }
    fputs("\r\n", fp);
    for (j = 0; j < file->sample_count; ++j) {
        for (i = 0; i < file->curve_count; ++i) {
            if (i) fputc('\t', fp);
            fprintf(fp, "%.7g", file->curves[i].values[j]);
        }
        fputs("\r\n", fp);
    }
    if (fclose(fp)) { set_error(err, errn, "关闭 SWX 文件失败"); return 0; }
    return 1;
}

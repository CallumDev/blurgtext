#include "util.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

size_t utf16_strlen(uint16_t *text)
{
    size_t sz = 0;
    while(*text++) {
        sz++;
    }
    return sz;
}

char *strlower(const char *name, int *length)
{
    char *lw = strdup(name);
    int l = 0;
    char *p = lw;
    while(*p) {
        *p = tolower(*p);
        l++;
        p++;
    }
    *length = l;
    return lw;
}
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
FILE *fopen_utf8(const char *filename, const char *mode)
{
    int filename_wsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
    int mode_wsize = MultiByteToWideChar(CP_UTF8, 0, mode, -1, NULL, 0);

    wchar_t *fbuf = malloc(filename_wsize * sizeof(wchar_t));
    wchar_t *mbuf = malloc(mode_wsize * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, filename, -1, fbuf, filename_wsize);
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, mbuf, mode_wsize);
    FILE *retval = _wfopen(fbuf, mbuf);
    free(fbuf);
    free(mbuf);
    return retval;
}
#else
#define fopen_utf8 fopen
#endif
unsigned char *read_all_bytes(const char *filename, size_t *length)
{
    FILE *f = fopen_utf8(filename, "rb");
    if(!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);

    unsigned char *buffer = malloc(len);
    fread(buffer, len, 1, f);
    fclose(f);
    *length = len;
    return buffer;
}
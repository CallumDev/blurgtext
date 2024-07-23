#ifndef _UTIL_H_
#define _UTIL_H_

#include <string.h>
#ifdef _MSC_VER
#define strdup _strdup
#define strcmp_i(a,b) _stricmp((a), (b))
#else
#include <strings.h>
#define strcmp_i(a,b) strcasecmp((a), (b))
#endif

#ifdef _WIN32
#include <malloc.h>
#define stackalloc _alloca
#else
#include <alloca.h>
#define stackalloc alloca
#endif
/* Creates a lowercase copy of the string, and puts the length of the string in length*/
char *strlower(const char *name, int *length);
/* Reads all bytes of filename into a memory buffer*/
unsigned char *read_all_bytes(const char *filename, size_t *length);
#endif
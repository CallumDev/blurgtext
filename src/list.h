#ifndef _LIST_H_
#define _LIST_H_
#include<stdlib.h>

#define LIST_DEF(structname, type) typedef struct { \
type *data; \
int capacity; \
int count; \
} structname; \
\
void structname ## _init (structname *list, int initial); \
void structname ## _ensure_size (structname *list, int capacity); \
void structname ## _insert (structname *list, int index, type data); \
void structname ## _add (structname *list, type data); \
void structname ## _add_range(structname *dst, structname *src); \
void structname ## _shrink(structname *list); \
void structname ## _free(structname *list);

#define LIST_IMPL(structname, type) \
void structname ## _init (structname *list, int initial) \
{\
    list->data = malloc(sizeof(type) * initial); \
    list->capacity = initial; \
    list->count = 0; \
}\
\
void structname ## _ensure_size (structname *list, int capacity) \
{ \
  if(list->capacity < capacity) { \
    list->data = realloc(list->data, sizeof(type) * capacity); \
    list->capacity = capacity; \
  } \
} \
\
void structname ## _insert (structname *list, int index, type data) \
{\
    if(index >= list->count) {\
        structname ## _ensure_size(list, index + 1); \
        list->count = index + 1; \
        list->data[index] = data; \
        return;\
    }\
    structname ## _ensure_size(list, list->count + 1); \
    for(int i = list->count; i > index; i--) { \
        list->data[i] = list->data[i - 1]; \
    } \
    list->data[index] = data; \
    list->count++; \
}\
void structname ## _add (structname *list, type data) \
{\
    structname ## _ensure_size(list, list->count + 1);\
    list->data[list->count++] = data; \
}\
void structname ## _add_range(structname *dst, structname *src) \
{\
    structname ## _ensure_size(dst, dst->count + src->count); \
    memcpy(&dst->data[dst->count], src->data, sizeof(type) * src->count); \
    dst->count += src->count; \
}\
void structname ## _shrink(structname *list) \
{\
    if(list->capacity > list->count) { \
        list->data = realloc(list->data, sizeof(type) * list->count); \
    } \
}\
void structname ## _free(structname *list) \
{ \
    free(list->data); \
}

#define DEFINE_LIST(type) LIST_DEF(list_ ## type, type)
#define IMPLEMENT_LIST(type) LIST_IMPL(list_ ## type, type)
#endif

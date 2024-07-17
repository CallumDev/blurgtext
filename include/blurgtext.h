#ifndef _BLURGTEXT_H_
#define _BLURGTEXT_H_
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#if BUILDING_BLURG
#define BLURGAPI __declspec(dllexport)
#else
#define BLURGAPI __declspec(dllimport)
#endif
#else
#define BLURGAPI __attribute__((visibility("default")))
#endif

#include <stdint.h>

typedef struct _blurg blurg_t;
typedef struct _blurg_font blurg_font_t;

typedef struct _blurg_texture {
    void* userdata;
} blurg_texture_t;

typedef struct _blurg_rect {
    blurg_texture_t* texture;
    int x;
    int y;
    int width;
    int height;
    float u0;
    float v0;
    float u1;
    float v1;
} blurg_rect_t;

typedef struct _blurg_style_span {
    int startIndex;
    int endIndex;
    blurg_font_t *font;
    float fontSize;
} blurg_style_span_t;

typedef enum {
	blurg_align_left = 0,
	blurg_align_right = 1,
	blurg_align_center = 2
} blurg_align_t;

typedef enum {
    blurg_encoding_utf8 = 0,
    blurg_encoding_utf16 = 1
} blurg_encoding_t;


typedef struct _blurg_formatted_text {
  const void *text;
  blurg_encoding_t encoding;
  blurg_align_t alignment;
  blurg_style_span_t *spans;
  int spanCount;
  float defaultSize;
  blurg_font_t* defaultFont;
} blurg_formatted_text_t;

typedef void (*blurg_texture_allocate)(blurg_texture_t *texture, int width, int height);
typedef void (*blurg_texture_update)(blurg_texture_t *texture, void *buffer, int x, int y, int width, int height);

BLURGAPI blurg_t *blurg_create(blurg_texture_allocate textureAllocate, blurg_texture_update textureUpdate);

BLURGAPI blurg_font_t *blurg_font_create(blurg_t *blurg, const char *filename);

BLURGAPI blurg_rect_t* blurg_build_string(blurg_t *blurg, blurg_font_t *font, float size, const char *text, int* rectCount);
BLURGAPI blurg_rect_t* blurg_build_formatted(blurg_t *blurg, blurg_formatted_text_t *text, float maxWidth, int *rectCount);

BLURGAPI void blurg_free_rects(blurg_rect_t *rects);

BLURGAPI void blurg_destroy(blurg_t *blurg);

#ifdef __cplusplus
}
#endif
#endif
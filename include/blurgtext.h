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

// Usually a 32-bit RGBA color, representing 0xRRGGBBAA.
// This is not modified by blurgtext.
typedef uint32_t blurg_color_t;
#define BLURG_RGBA(r,g,b,a) (\
    (uint32_t)((a) << 24) | \
    (uint32_t)((b) << 16) | \
    (uint32_t)((g) << 8) | \
    (uint32_t)((r)) \
)

typedef struct _blurg_underline {
    blurg_color_t color;
    char useColor;
    char enabled;
} blurg_underline_t;

#define BLURG_UNDERLINED ((blurg_underline_t){ .color = 0, .useColor = 0, .enabled = 1 })
#define BLURG_NO_UNDERLINE ((blurg_underline_t){ .enabled = 0 })

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
    blurg_color_t color;
} blurg_rect_t;

typedef struct _blurg_style_span {
    int startIndex;
    int endIndex;
    blurg_font_t *font;
    float fontSize;
    blurg_color_t color;
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
  blurg_color_t defaultColor;
} blurg_formatted_text_t;

typedef void (*blurg_texture_allocate)(blurg_texture_t *texture, int width, int height);
typedef void (*blurg_texture_update)(blurg_texture_t *texture, void *buffer, int x, int y, int width, int height);

BLURGAPI blurg_t *blurg_create(blurg_texture_allocate textureAllocate, blurg_texture_update textureUpdate);

BLURGAPI blurg_font_t *blurg_font_create(blurg_t *blurg, const char *filename);

BLURGAPI blurg_rect_t* blurg_build_string(blurg_t *blurg, blurg_font_t *font, float size, blurg_color_t color, const char *text, int* rectCount);
BLURGAPI blurg_rect_t* blurg_build_formatted(blurg_t *blurg, blurg_formatted_text_t *text, float maxWidth, int *rectCount);

BLURGAPI void blurg_free_rects(blurg_rect_t *rects);

BLURGAPI void blurg_destroy(blurg_t *blurg);

#ifdef __cplusplus
}
#endif
#endif
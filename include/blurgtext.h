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

#define BLURG_WEIGHT_THIN (100)
#define BLURG_WEIGHT_EXTRALIGHT (200)
#define BLURG_WEIGHT_LIGHT (300)
#define BLURG_WEIGHT_REGULAR (400)
#define BLURG_WEIGHT_MEDIUM (500)
#define BLURG_WEIGHT_SEMIBOLD (600)
#define BLURG_WEIGHT_BOLD (700)
#define BLURG_WEIGHT_EXTRABOLD (800)
#define BLURG_WEIGHT_BLACK (900)

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
    int useColor;
    int enabled;
} blurg_underline_t;

#define BLURG_UNDERLINED ((blurg_underline_t){ .color = 0, .useColor = 0, .enabled = 1 })
#define BLURG_NO_UNDERLINE ((blurg_underline_t){ .enabled = 0 })

typedef struct _blurg_shadow {
    int pixels;
    blurg_color_t color;
} blurg_shadow_t;

#define BLURG_NO_SHADOW ((blurg_shadow_t) { .color = 0, .pixels = 0 })

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
    blurg_color_t background;
    blurg_color_t color;
    blurg_underline_t underline;
    blurg_shadow_t shadow;
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
  blurg_color_t defaultBackground;
  blurg_color_t defaultColor;
  blurg_underline_t defaultUnderline;
  blurg_shadow_t defaultShadow;
} blurg_formatted_text_t;

typedef void (*blurg_texture_allocate)(blurg_texture_t *texture, int width, int height);
typedef void (*blurg_texture_update)(blurg_texture_t *texture, void *buffer, int x, int y, int width, int height);

BLURGAPI blurg_t *blurg_create(blurg_texture_allocate textureAllocate, blurg_texture_update textureUpdate);

BLURGAPI const char *blurg_font_get_family(blurg_font_t *font);
BLURGAPI int blurg_font_get_italic(blurg_font_t *font);
BLURGAPI int blurg_font_get_weight(blurg_font_t *font);
BLURGAPI float blurg_font_get_line_height(blurg_font_t *font, float size);

BLURGAPI blurg_font_t *blurg_font_add_file(blurg_t *blurg, const char *filename);
/*
 * Adds a font from a memory buffer. 
 * If copy is 1, blurg copies the data internally.
 * If copy is 0, the application manages the data. The buffer must not be freed until after blurg_destroy is called.
*/
BLURGAPI blurg_font_t *blurg_font_add_memory(blurg_t *blurg, char *data, int len, int copy);
/*
* Tries to find a font with the specified family, weight and italic.
* Returns NULL if a font is not found
*/
BLURGAPI blurg_font_t *blurg_font_query(blurg_t *blurg, const char *familyName, int weight, int italic);
/*
 * Turns a single string into a rectangle array. Free the result with blurg_free_rects
*/
BLURGAPI blurg_rect_t* blurg_build_string(blurg_t *blurg, blurg_font_t *font, float size, blurg_color_t color, const char *text, int* rectCount, float *width, float *height);
BLURGAPI blurg_rect_t* blurg_build_string_utf16(blurg_t *blurg, blurg_font_t *font, float size, blurg_color_t color, const uint16_t *text, int* rectCount, float *width, float *height);
/*
 * Turns an array of formatted text objects into a rectangle array. Free the result with blurg_free_rects
 * Count of rects is written to rectCount, width and height of built text written to width/height parameters
 * This function does not take ownership of any members of blurg_formatted_text_t
*/
BLURGAPI blurg_rect_t* blurg_build_formatted(blurg_t *blurg, blurg_formatted_text_t *texts, int count, float maxWidth, int *rectCount, float *width, float *height);

/*
 * Measures the provided string, size is written to width+height
*/
BLURGAPI void blurg_measure_string(blurg_t *blurg, blurg_font_t *font, float size, const char *text, float *width, float *height);
BLURGAPI void blurg_measure_string_utf16(blurg_t *blurg, blurg_font_t *font, float size, const uint16_t *text, float *width, float *height);
/*
 * Measures the provided formatted texts, size is written to width+height
*/
BLURGAPI void blurg_measure_formatted(blurg_t *blurg, blurg_formatted_text_t *texts, int count, float maxWidth, float* width, float *height);

/*
 * Frees a rectangle array returned by a  blurg_build_* function
*/
BLURGAPI void blurg_free_rects(blurg_rect_t *rects);
/*
 * Destroys a blurg instance and all associated fonts.
 * Does NOT free rectangle arrays returned from blurg_free_*
*/
BLURGAPI void blurg_destroy(blurg_t *blurg);

#ifdef __cplusplus
}
#endif
#endif
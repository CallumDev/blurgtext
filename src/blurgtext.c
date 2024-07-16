#include <raqm.h>
#include "blurgtext_internal.h"
#include <string.h>

BLURGAPI blurg_t *blurg_create(blurg_texture_allocate textureAllocate, blurg_texture_update textureUpdate)
{
    blurg_t *blurg = malloc(sizeof(blurg_t));
    memset(blurg, 0, sizeof(blurg_t));
    blurg->textureAllocate = textureAllocate;
    blurg->textureUpdate = textureUpdate;
    FT_Error error = FT_Init_FreeType(&blurg->library);
    if(error) {
        printf("FT_Init_FreeType failed\n");
        free(blurg);
        return NULL;
    }
    glyphatlas_init(blurg);
    return blurg;
}

BLURGAPI void blurg_destroy(blurg_t *blurg)
{
    glyphatlas_destroy(blurg);
}

typedef struct rect_buffer {
    blurg_rect_t *data;
    size_t capacity;
    size_t count;
} rect_buffer;

static void rect_buffer_new(rect_buffer *rb, size_t initial)
{
    rb->count = 0;
    rb->capacity = initial;
    rb->data = malloc(rb->capacity * sizeof(blurg_rect_t));
}

static void rect_buffer_ensure_capacity(rect_buffer *rb, size_t capacity)
{
    if(capacity > rb->capacity) {
        rb->capacity = capacity;
        rb->data = realloc(rb->data, rb->capacity * sizeof(blurg_rect_t));
    }
}

static void rect_buffer_shrink(rect_buffer *rb)
{
    if(rb->count < rb->capacity) {
        rb->capacity = rb->count;
        rb->data = realloc(rb->data, rb->capacity * sizeof(blurg_rect_t));
    }
}

static void blurg_shape(blurg_t *blurg, blurg_font_t *font, const char *str, int len, rect_buffer *rb, float x, float y)
{
    raqm_t* rq = raqm_create();
    if( raqm_set_text_utf8(rq, str, len) &&
        raqm_set_freetype_face(rq, font->face) &&
        raqm_set_par_direction(rq, RAQM_DIRECTION_DEFAULT) &&
        raqm_layout(rq)) {
        size_t count;
        raqm_glyph_t *glyphs = raqm_get_glyphs (rq, &count);
        rect_buffer_ensure_capacity(rb, rb->count + count);
        for(int i = 0; i < count; i++) {
            blurg_glyph vis;
            glyphatlas_get(blurg, font, glyphs[i].index, &vis);
            blurg_rect_t *r = &rb->data[rb->count++];
            r->texture = blurg->packed.pages[vis.texture];
            r->u0 = vis.srcX / (float)BLURG_TEXTURE_SIZE;
            r->v0 = vis.srcY / (float)BLURG_TEXTURE_SIZE;
            r->u1 = (vis.srcX + vis.srcW) / (float)BLURG_TEXTURE_SIZE;
            r->v1 = (vis.srcY + vis.srcH) / (float)BLURG_TEXTURE_SIZE;
            r->x = (int)(x + (glyphs[i].x_offset / 64.0) + vis.offsetLeft);
            r->y = (int)(y + (glyphs[i].y_offset / 64.0) + font->ascender - vis.offsetTop);
            r->width = vis.srcW;
            r->height = vis.srcH;
            x += glyphs[i].x_advance / 64.0;
            y += glyphs[i].y_advance / 64.0;
        }
    }
    raqm_destroy(rq);
}

BLURGAPI blurg_rect_t* blurg_buildstring(blurg_t *blurg, blurg_font_t *font, float size, const char *text, int* rectCount)
{
    int total = strlen(text);
    if(total == 0) {
        *rectCount = 0;
        return NULL;
    }
    font_use_size(font, size);
    rect_buffer rb;
    rect_buffer_new(&rb, total);
    int last = 0;
    float y = 0;
    for(int i = 0; i < total; i++) {
        if(text[i] == '\n') {
            if(i - last > 0)  {
                blurg_shape(blurg, font, &text[last], i - last, &rb, 0, y);
            }
            y += font->lineHeight;
            last = i + 1;
        }
    }
    if(total - last > 0) {
        blurg_shape(blurg, font, &text[last], total - last, &rb, 0, y);
    }
    if(rb.count > 0) {
        rect_buffer_shrink(&rb);
        *rectCount = rb.count;
        return rb.data;
    } else {
        free(rb.data);
        *rectCount = 0;
        return NULL;
    }
}

BLURGAPI void blurg_free_rects(blurg_rect_t *rects)
{
    free(rects);
}

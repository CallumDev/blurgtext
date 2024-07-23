#include <raqm.h>
#include "blurgtext_internal.h"
#include <string.h>
#include <linebreak.h>
#include "list.h"
#include "util.h"
#include <math.h>

DEFINE_LIST(blurg_rect_t);
IMPLEMENT_LIST(blurg_rect_t);

typedef struct {
    int isBreak;
    int textStart;
    int textCount;
    float width;
    float lineHeight;
    int paraIndex;
    list_blurg_rect_t rects;
} text_line;

DEFINE_LIST(text_line);
IMPLEMENT_LIST(text_line);


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
    font_manager_init(blurg);
    return blurg;
}

BLURGAPI void blurg_destroy(blurg_t *blurg)
{
    glyphatlas_destroy(blurg);
    FT_Done_Library(blurg->library);
    font_manager_destroy(blurg);
    free(blurg);
}

static size_t utf16_strlen(utf16_t *text)
{
    size_t sz = 0;
    while(*text++) {
        sz++;
    }
    return sz;
}

static void blurg_get_lines(const void *text, int* textLen, blurg_encoding_t encoding, list_text_line *lines, char **breaks, int paraIndex)
{
    int total;
    if(encoding == blurg_encoding_utf16) {
        total = utf16_strlen((utf16_t*)text);
        *breaks = malloc(total);
        set_linebreaks_utf16((utf16_t*)text, total, NULL, *breaks);
    }
    else {
        total = strlen((char*)text);
        *breaks = malloc(total);
        set_linebreaks_utf8(text, total, NULL, *breaks);
    }
    *textLen = total;
    int last = 0;
    for(int i = 0; i < total; i++) {
        if((*breaks)[i] == LINEBREAK_MUSTBREAK) {
            if(last == i) {
                list_text_line_add(lines, (text_line){ .isBreak = 1, .textStart = i, .textCount = 1, .paraIndex = paraIndex});
            } else {
                list_text_line_add(lines, (text_line){ .isBreak = 0, .textStart = last, .textCount = i - last, .paraIndex = paraIndex});
            }
            last = i + 1;
        }
    }
    if(total - last > 0) {
        list_text_line_add(lines, (text_line){.isBreak = 0, .textStart = last, .textCount = total - last, .paraIndex = paraIndex });
    }
}

// style accessors
#define IDX_FONT(i) ((!attributes || attributes[(i)] == -1) ? text->defaultFont : text->spans[attributes[(i)]].font)
#define IDX_SIZE(i) ((!attributes || attributes[(i)] == -1) ? text->defaultSize : text->spans[attributes[(i)]].fontSize)
#define IDX_COLOR(i) ((!attributes || attributes[(i)] == -1) ? text->defaultColor : text->spans[attributes[(i)]].color)
#define IDX_UNDERLINE(i) ((!attributes || attributes[(i)] == -1) ? text->defaultUnderline : text->spans[attributes[(i)]].underline)
#define IDX_SHADOW(i) ((!attributes || attributes[(i)] == -1) ? text->defaultShadow : text->spans[attributes[(i)]].shadow)

typedef struct {
    int active;
    blurg_color_t color;
    float xStart;
    float pos;
    int offset;
} active_underline;

static void add_underline(blurg_t *b, active_underline ul, float xEnd, list_blurg_rect_t *rb, float y)
{
    list_blurg_rect_t_add(rb, (blurg_rect_t){
        .texture = rb->count > 0 ? rb->data[rb->count - 1].texture : b->packed.pages[0],
        .u0 = 0.5 / BLURG_TEXTURE_SIZE, //sample centre of pixel, works better.
        .u1 = 0.5 / BLURG_TEXTURE_SIZE,
        .v0 = 0.5 / BLURG_TEXTURE_SIZE,
        .v1 = 0.5 / BLURG_TEXTURE_SIZE,
        .x = (int)(ul.xStart + ul.offset),
        .y = (int)(ul.pos + y + ul.offset),
        .width = (int)(xEnd - ul.xStart),
        .height = 1,
        .color = ul.color,
    });
}

static void raqm_to_rects(
    blurg_t *blurg, 
    raqm_t *rq, 
    list_blurg_rect_t *rb, 
    float *x, 
    float *y, 
    size_t maxGlyph, 
    blurg_formatted_text_t *text, 
    int* attributes,
    int hasShadow,
    int hasUnderline)
{
    size_t count;
    raqm_glyph_t *glyphs = raqm_get_glyphs (rq, &count);
    list_blurg_rect_t_ensure_size(rb, rb->count + count); //optimize to save reallocs
    active_underline ul = { .active = 0 };
    // draw shadows
    if(hasShadow) {
        float shadowX = *x;
        float shadowY = *y;
        for(int i = 0; i < count && i < maxGlyph; i++) {
            blurg_shadow_t shadow = IDX_SHADOW(glyphs[i].cluster);
            if(!shadow.pixels) {
                if(ul.active) {
                    add_underline(blurg, ul, shadowX, rb, shadowY);
                    ul.active = 0;
                }
                shadowX += glyphs[i].x_advance / 64.0;
                shadowY += glyphs[i].y_advance / 64.0;
                continue;
            }
            //glyph shadow
            blurg_glyph vis;
            blurg_font_t *font = blurg_from_freetype(glyphs[i].ftface);
            glyphatlas_get(blurg, font, glyphs[i].index, &vis);
            list_blurg_rect_t_add(rb, (blurg_rect_t) {
                .texture = blurg->packed.pages[vis.texture],
                .u0 = vis.srcX / (float)BLURG_TEXTURE_SIZE,
                .v0 = vis.srcY / (float)BLURG_TEXTURE_SIZE,
                .u1 = (vis.srcX + vis.srcW) / (float)BLURG_TEXTURE_SIZE,
                .v1 = (vis.srcY + vis.srcH) / (float)BLURG_TEXTURE_SIZE,
                .x = shadow.pixels + (int)(shadowX + (glyphs[i].x_offset / 64.0) + vis.offsetLeft),
                .y = shadow.pixels + (int)(shadowY + (glyphs[i].y_offset / 64.0) - vis.offsetTop),
                .width = vis.srcW,
                .height = vis.srcH,
                .color = shadow.color, 
            });
            if(!hasUnderline)
                continue;
            //underline shadow
            blurg_underline_t underline = IDX_UNDERLINE(glyphs[i].cluster);
            float upos;
            if(underline.enabled) {
                float sz = (font->setSize / 64.0);
                upos = (font->face->underline_position / (64.0 * 64.0)) * sz;
                upos = -roundf(upos);
            }
            if(!ul.active && underline.enabled) {
                ul.active = 1;
                ul.xStart = shadowX;
                ul.color = shadow.color;
                ul.offset = shadow.pixels;
            }
            else if(ul.active && !underline.enabled) {
                add_underline(blurg, ul, shadowX, rb, shadowY);
                ul.active = 0;
            }
            else if (ul.active && underline.enabled && (
                shadow.color != ul.color ||
                shadow.pixels != ul.offset ||
                upos != ul.pos)) {
                add_underline(blurg, ul, shadowX, rb, shadowY);
                ul.xStart = shadowX;
                ul.color = shadow.color;
                ul.offset = shadow.pixels;
            }
            shadowX += glyphs[i].x_advance / 64.0;
            shadowY += glyphs[i].y_advance / 64.0;
        }
        if(ul.active) {
            add_underline(blurg, ul, shadowX, rb, *y);
        }
        ul.active = 0;
    }
    
    // draw underlines (not shadowed)
    if(hasUnderline) {
        float underlineX = *x;
        float underlineY = *y;
        for(int i = 0; i < count && i < maxGlyph; i++) {
            blurg_underline_t underline = IDX_UNDERLINE(glyphs[i].cluster);
            blurg_font_t *font;
            uint32_t ucolor;
            float upos;
            if(underline.enabled) {
                ucolor = underline.useColor ? underline.color : IDX_COLOR(glyphs[i].cluster);
                font = blurg_from_freetype(glyphs[i].ftface);
                float sz = (font->setSize / 64.0);
                upos = (font->face->underline_position / (64.0 * 64.0)) * sz;
                upos = -roundf(upos);
            }
            if(!ul.active && underline.enabled) {
                ul.active = 1;
                ul.xStart = underlineX;
                ul.color = ucolor;
                ul.offset = 0;
                ul.pos = upos;
            }
            else if(ul.active && !underline.enabled) {
                add_underline(blurg, ul, underlineX, rb, underlineY);
                ul.active = 0;
            }
            else if (ul.active && underline.enabled && (
                ucolor != ul.color ||
                upos != ul.pos)) {
                add_underline(blurg, ul, underlineX, rb, underlineY);
                ul.xStart = *x;
                ul.color = ucolor;
                ul.offset = 0;
                ul.pos = upos;
            }
            underlineX += glyphs[i].x_advance / 64.0;
            underlineY += glyphs[i].y_advance / 64.0;
        }
        if(ul.active) {
            add_underline(blurg, ul, underlineX, rb, underlineY);
        }
    }

    // finally, draw colour glyphs on top
    
    for(int i = 0; i < count && i < maxGlyph; i++) {

        blurg_glyph vis;
        blurg_font_t *font = blurg_from_freetype(glyphs[i].ftface);
        glyphatlas_get(blurg, font, glyphs[i].index, &vis);
        list_blurg_rect_t_add(rb, (blurg_rect_t) {
            .texture = blurg->packed.pages[vis.texture],
            .u0 = vis.srcX / (float)BLURG_TEXTURE_SIZE,
            .v0 = vis.srcY / (float)BLURG_TEXTURE_SIZE,
            .u1 = (vis.srcX + vis.srcW) / (float)BLURG_TEXTURE_SIZE,
            .v1 = (vis.srcY + vis.srcH) / (float)BLURG_TEXTURE_SIZE,
            .x = (int)(*x + (glyphs[i].x_offset / 64.0) + vis.offsetLeft),
            .y = (int)(*y + (glyphs[i].y_offset / 64.0) - vis.offsetTop),
            .width = vis.srcW,
            .height = vis.srcH,
            .color = IDX_COLOR(glyphs[i].cluster), 
        });
        *x += glyphs[i].x_advance / 64.0;
        *y += glyphs[i].y_advance / 64.0;
    }
    
}

static void wrap_line(raqm_glyph_t *glyphs, char* breaks, int *charCount, size_t *glyphCount, float x, float maxWidth)
{
    if(maxWidth > 0) {
        // wrap text if necessary
        float cx = x;
        size_t stop;
        for(stop = 0; stop < *glyphCount; stop++) {
            float advance = (glyphs[stop].x_advance / 64.0);
            if(cx + advance > maxWidth) {
                break;
            }
            cx += advance;
        }
        // Text is too big, we need to line break!
        if(stop != *glyphCount) {
            if(x <= 0 && stop == 0) { 
                // too big to put a single character on a line?
                // just output one glyph
                stop = 1;
                *glyphCount = 1;
                *charCount = (int)glyphs[1].cluster;
            } 
            else {
                // Calculate an appropriate line break
                int cc = (int)glyphs[stop].cluster;
                int lastAllowed = -1;
                for(int i = cc - 1; i > 0; i--) {
                    if(breaks[i] == LINEBREAK_ALLOWBREAK) {
                        lastAllowed = i;
                        break;
                    }
                }
                // If we have an appropiate line break, use that
                if(lastAllowed != -1) {
                    int found = 0;
                    for(int i = 0; i < stop; i++) {
                        if(glyphs[i].cluster == lastAllowed) {
                            stop = i;
                            found = 1;
                            break;
                        }
                    }
                    if(found && (stop + 1) < *glyphCount) {
                        //skip the space we converted into a break
                        *charCount = (int)glyphs[stop + 1].cluster;
                    } else {
                        *charCount = (int)glyphs[stop].cluster;
                    }
                    *glyphCount = stop;
                } else {
                    //no appropriate line break, just insert an \n
                    *glyphCount = stop;
                    *charCount = (int)glyphs[stop].cluster;
                }
            }
        }
    }
}

// returns the length of text shaped/turned into rects for current maxWidth
static int blurg_shape_chunk(blurg_t *blurg, const void *str, char *breaks, int len, float size,
    int* attributes, blurg_formatted_text_t *text, list_blurg_rect_t *rb, float *x, float *y, float maxWidth)
{
    raqm_t* rq = raqm_create();
    if(text->encoding == blurg_encoding_utf16)
        raqm_set_text_utf16(rq, (const utf16_t*)str, len);
    else
        raqm_set_text_utf8(rq, (const char*)str, len);
    raqm_set_par_direction(rq, RAQM_DIRECTION_DEFAULT);
    int hasShadows = len + 1;
    int hasUnderlines = len + 1;
    for(int i = 0; i < len; i++) {
        blurg_font_t *fontAtIndex = IDX_FONT(i);
        // optimisation. check if there are any shadow/underline attributes
        // in the range during first loop. saves loops later 
        if((hasShadows > len) && IDX_SHADOW(i).pixels != 0) {
            hasShadows = i;
        }
        if((hasUnderlines > len) && IDX_UNDERLINE(i).enabled != 0) {
            hasUnderlines = i;
        }
        font_use_size(fontAtIndex, size);
        raqm_set_freetype_face_range(rq, fontAtIndex->face, i, 1);
    }
    raqm_layout(rq);
    size_t count = SIZE_MAX;
    int charCount = len;
    raqm_glyph_t *glyphs = raqm_get_glyphs (rq, &count);
    wrap_line(glyphs, breaks, &charCount, &count, *x, maxWidth);
    raqm_to_rects(blurg, rq, rb, x, y, count, text, attributes, hasShadows < charCount, hasUnderlines < charCount);
    raqm_destroy(rq);
    return charCount;
}

static void split_line(list_text_line *lines, int lineIndex, int splitPoint)
{
    int newStart = lines->data[lineIndex].textStart + splitPoint;
    int newCount = lines->data[lineIndex].textCount - splitPoint;
    lines->data[lineIndex].textCount = splitPoint;
    text_line newLine = {
        .isBreak = 0,
        .textStart = newStart,
        .textCount = newCount,
        .paraIndex = lines->data[lineIndex].paraIndex,
    };
    list_text_line_insert(lines, lineIndex + 1, newLine);
}

// accessors for attribute and text arrays
#define TEXT_OFFSET(x) (text->encoding == blurg_encoding_utf16 ? (const void*)(\
    &((const utf16_t*)(text->text))[(x)] \
) : (const void*) (\
    &((const char*)(text->text))[(x)] \
))
#define ATTRIBUTE_OFFSET(x) (attributes ? &attributes[(x)] : NULL)

static void blurg_wrap_shape_line(
    blurg_t *blurg, 
    blurg_formatted_text_t *text, 
    int* attributes, 
    list_text_line *lines,
    char *breaks,
    int lineIndex,
    float maxWidth)
{
    if(lines->data[lineIndex].isBreak) {
        // This line is just an \n.
        // Set line height and early exit
        lines->data[lineIndex].width = 0;
        blurg_font_t *fnt = IDX_FONT(lines->data[lineIndex].textStart);
        font_use_size(fnt, IDX_SIZE(lines->data[lineIndex].textStart));
        lines->data[lineIndex].lineHeight = fnt->lineHeight;
        lines->data[lineIndex].rects.count = 0;
        lines->data[lineIndex].rects.data = NULL;
    }

    list_blurg_rect_t shapeBuffer;
    list_blurg_rect_t_init(&shapeBuffer, lines->data[lineIndex].textCount);

    int start = lines->data[lineIndex].textStart;
    int count = lines->data[lineIndex].textCount;

    int last = 0;
    
    blurg_font_t *font0 = IDX_FONT(start);
    float currentSize = IDX_SIZE(start);
    font_use_size(font0, currentSize);

    float maxAscender = font0->ascender;
    float maxLineHeight = font0->lineHeight;

    float x = 0;
    float y = 0;

    for(int i = 0; i < count; i++) {
        blurg_font_t *fontAtIndex = IDX_FONT(start + i);
        float sizeAtIndex = IDX_SIZE(start + i);
        // size change, we need to shape text
        if(currentSize != sizeAtIndex) {
            if(i - last > 0) {
                int shapeCount = i - last;
                int actualCount = blurg_shape_chunk(
                    blurg, 
                    TEXT_OFFSET(start+last),
                    &breaks[start + last],
                    i - last, 
                    currentSize, 
                    ATTRIBUTE_OFFSET(start + last), 
                    text, 
                    &shapeBuffer, &x, &y, maxWidth);
                if(actualCount != shapeCount) {
                    split_line(lines, lineIndex, last + actualCount);
                    // early exit
                    goto finish;
                }
                last = i;
            }
        }
        //keep track of line height + ascender for line
        currentSize = sizeAtIndex;
        font_use_size(fontAtIndex, sizeAtIndex);
        if(fontAtIndex->lineHeight > maxLineHeight)
            maxLineHeight = fontAtIndex->lineHeight;
        if(fontAtIndex->ascender > maxAscender)
            maxAscender = fontAtIndex->ascender;
    }

    if(count - last > 0) {
        int shapeCount = count - last;
        int actualCount = blurg_shape_chunk(
            blurg, 
            TEXT_OFFSET(start + last), 
            &breaks[start + last], 
            count - last, 
            currentSize, 
            ATTRIBUTE_OFFSET(start + last), 
            text, 
            &shapeBuffer, &x, &y, maxWidth);
        if(actualCount != shapeCount) {
            split_line(lines, lineIndex, last + actualCount);
        }
    }


    finish:
    // apply ascender for line
    for(int i = 0; i < shapeBuffer.count; i++) {
        shapeBuffer.data[i].y += maxAscender;
    }
    // set metrics
    lines->data[lineIndex].width = x;
    lines->data[lineIndex].lineHeight = maxLineHeight;
    lines->data[lineIndex].rects = shapeBuffer;
}

static int blurg_measure_chunk(blurg_t *blurg, const void *str, char *breaks, int len, float size,
    int* attributes, blurg_formatted_text_t *text, float *x, float *y, float maxWidth)
{
    raqm_t* rq = raqm_create();
    if(text->encoding == blurg_encoding_utf16)
        raqm_set_text_utf16(rq, (const utf16_t*)str, len);
    else
        raqm_set_text_utf8(rq, (const char*)str, len);
    raqm_set_par_direction(rq, RAQM_DIRECTION_DEFAULT);
    for(int i = 0; i < len; i++) {
        blurg_font_t *fontAtIndex = IDX_FONT(i);
        font_use_size(fontAtIndex, size);
        raqm_set_freetype_face_range(rq, fontAtIndex->face, i, 1);
    }
    raqm_layout(rq);
    size_t count = SIZE_MAX;
    int charCount = len;
    raqm_glyph_t *glyphs = raqm_get_glyphs (rq, &count);
    wrap_line(glyphs, breaks, &charCount, &count, *x, maxWidth);
    for(int i = 0; i < count; i++) {
        *x += (glyphs[i].x_advance / 64.0);
        *y += (glyphs[i].y_advance / 64.0);
    }
    raqm_destroy(rq);
    return charCount;
}

static void blurg_measure_line(
    blurg_t *blurg, 
    blurg_formatted_text_t *text, 
    int* attributes, 
    list_text_line *lines,
    char *breaks,
    int lineIndex,
    float maxWidth
)
{
    if(lines->data[lineIndex].isBreak) {
        // This line is just an \n.
        // Set line height and early exit
        lines->data[lineIndex].width = 0;
        blurg_font_t *fnt = IDX_FONT(lines->data[lineIndex].textStart);
        font_use_size(fnt, IDX_SIZE(lines->data[lineIndex].textStart));
        lines->data[lineIndex].lineHeight = fnt->lineHeight;
        lines->data[lineIndex].rects.count = 0;
        lines->data[lineIndex].rects.data = NULL;
        return;
    }

    int start = lines->data[lineIndex].textStart;
    int count = lines->data[lineIndex].textCount;

    int last = 0;
    
    blurg_font_t *font0 = IDX_FONT(start);
    float currentSize = IDX_SIZE(start);
    font_use_size(font0, currentSize);

    float maxAscender = font0->ascender;
    float maxLineHeight = font0->lineHeight;

    float x = 0;
    float y = 0;

    for(int i = 0; i < count; i++) {
        blurg_font_t *fontAtIndex = IDX_FONT(start + i);
        float sizeAtIndex = IDX_SIZE(start + i);
        // size change, we need to shape text
        if(currentSize != sizeAtIndex) {
            if(i - last > 0) {
                int shapeCount = i - last;
                int actualCount = blurg_measure_chunk(
                    blurg, 
                    TEXT_OFFSET(start+last),
                    &breaks[start + last],
                    i - last, 
                    currentSize, 
                    ATTRIBUTE_OFFSET(start + last), 
                    text, 
                    &x, &y, maxWidth);
                if(actualCount != shapeCount) {
                    split_line(lines, lineIndex, last + actualCount);
                    // early exit
                    goto finish;
                }
                last = i;
            }
        }
        //keep track of line height + ascender for line
        currentSize = sizeAtIndex;
        font_use_size(fontAtIndex, sizeAtIndex);
        if(fontAtIndex->lineHeight > maxLineHeight)
            maxLineHeight = fontAtIndex->lineHeight;
        if(fontAtIndex->ascender > maxAscender)
            maxAscender = fontAtIndex->ascender;
    }

    if(count - last > 0) {
        int shapeCount = count - last;
        int actualCount = blurg_measure_chunk(
            blurg, 
            TEXT_OFFSET(start + last), 
            &breaks[start + last], 
            count - last, 
            currentSize, 
            ATTRIBUTE_OFFSET(start + last), 
            text, 
            &x, &y, maxWidth);
        if(actualCount != shapeCount) {
            split_line(lines, lineIndex, last + actualCount);
        }
    }

    finish:
    // set metrics
    lines->data[lineIndex].width = x;
    lines->data[lineIndex].lineHeight = maxLineHeight;    
}

#undef TEXT_OFFSET
#undef ATTRIBUTE_OFFSET

typedef struct _paragraph_info {
    int lineOffset;
    int total;
    int *attributes;
    char *breaks;
} paragraph_info;

#define ALLOC_GUARDED(count,sz) (((count * sz) < 1024) ? stackalloc(count * sz) : malloc(count * sz))
#define DEALLOC_GUARDED(x, count,sz) if (((count) * (sz)) >= 1024) free((x))

BLURGAPI blurg_rect_t* blurg_build_formatted(blurg_t *blurg, blurg_formatted_text_t *texts, int count, float maxWidth, int *rectCount, float *width, float *height)
{
    list_text_line lines;
    list_text_line_init(&lines, 8);

    int sumParagraphs = 0;

    paragraph_info* paragraphs = ALLOC_GUARDED(count, sizeof(paragraph_info));

    // Build info and process mandatory line breaks
    int hasAlign = 0;
    for(int i = 0; i < count; i++) {
        paragraphs[i].lineOffset = lines.count;
        blurg_get_lines(texts[i].text, &paragraphs[i].total, texts[i].encoding, &lines, &paragraphs[i].breaks, i);
        sumParagraphs += paragraphs[i].total;
        if(paragraphs[i].total && texts[i].alignment != blurg_align_left) {
            hasAlign = 1;
        }
        if(texts[i].spans && texts[i].spanCount) {
            int* attributes = malloc(paragraphs[i].total * sizeof(int));
            for(int j = 0; j < paragraphs[i].total; j++) 
            {
                attributes[j] = -1;
            }
            for(int j = 0; j < texts[i].spanCount; j++) 
            {
                for(int k = texts[i].spans[j].startIndex; k <= texts[i].spans[j].endIndex; k++) 
                {
                    attributes[k] = j;
                }
            }
            paragraphs[i].attributes = attributes;
        } else {
            paragraphs[i].attributes = NULL;
        }
    }

    float alignWidth = maxWidth;
    if(hasAlign) {
        for(int i = 0; i < lines.count; i++) {
            int para = lines.data[i].paraIndex;
            blurg_wrap_shape_line(blurg, &texts[para], paragraphs[para].attributes, &lines, paragraphs[para].breaks, i, maxWidth);
            if(lines.data[i].width > alignWidth) {
                alignWidth = lines.data[i].width;
            }
        }
    }

    // lines to shaped text
    list_blurg_rect_t rects;
    list_blurg_rect_t_init(&rects, sumParagraphs);
    float y = 0;
    float w = 0;
    for(int i = 0; i < lines.count; i++) {
        int pIdx = lines.data[i].paraIndex;
        if(!hasAlign) //we don't need to allocate line buffers upfront if left aligned
        {
            blurg_wrap_shape_line(blurg, &texts[pIdx], paragraphs[pIdx].attributes, &lines, paragraphs[pIdx].breaks, i, maxWidth);
        }
        if(!lines.data[i].isBreak) {
            // copy rects
            float offsetW = 0;
            if(texts[pIdx].alignment == blurg_align_right) {
                offsetW = (alignWidth - lines.data[i].width);
            }
            if(texts[pIdx].alignment == blurg_align_center) {
                offsetW = (alignWidth / 2.0) - (lines.data[i].width / 2.0);
            }
            for(int j = 0; j < lines.data[i].rects.count; j++) {
                //process alignment
                lines.data[i].rects.data[j].x += offsetW;
                //position line
                lines.data[i].rects.data[j].y += y;
            }

            if((lines.data[i].width + offsetW) > w)
                w = (lines.data[i].width + offsetW);
            list_blurg_rect_t_add_range(&rects, &lines.data[i].rects);
            list_blurg_rect_t_free(&lines.data[i].rects);
        }
        y += lines.data[i].lineHeight;
    }
    //cleanup and return
    for(int i = 0; i < count; i++) {
        free(paragraphs[i].breaks);
        if(paragraphs[i].attributes)
            free(paragraphs[i].attributes);  
    }
    DEALLOC_GUARDED(paragraphs, count, sizeof(paragraph_info));
    list_text_line_free(&lines);
    
    if(width)
        *width = w;
    if(height)
        *height = y;

    if(rects.count > 0) {
        list_blurg_rect_t_shrink(&rects);
        *rectCount = rects.count;
        return rects.data;
    } else {
        list_blurg_rect_t_free(&rects);
        *rectCount = 0;
        return NULL;
    }
}

BLURGAPI void blurg_measure_formatted(blurg_t *blurg, blurg_formatted_text_t *texts, int count, float maxWidth, float* width, float *height)
{
    if(!width && !height)
        return;

    int total;
    list_text_line lines;
    list_text_line_init(&lines, 2 * count);
    char *breaks;

    // measure all paragraphs
    float h = 0;
    float w = 0;    
    
    int hasAlign = 0;
    float alignWidth = maxWidth;

    for(int i = 0; i < count; i++) {
        int startIdx = lines.count;
        blurg_get_lines(texts[i].text, &total, texts[i].encoding, &lines, &breaks, i);
        if(!total) {
            free(breaks);
            continue;
        }
        if(texts[i].alignment == blurg_align_center ||
           texts[i].alignment == blurg_align_right) {
            hasAlign = 1;
        }
        // Attribute lookup table
        int* attributes = NULL;
        if(texts[i].spans && texts[i].spanCount) {
            attributes = malloc(total * sizeof(int));
            for(int j = 0; j < total; j++) 
            {
                attributes[j] = -1;
            }
            for(int j = 0; j < texts[i].spanCount; j++) {
                for(int k = texts[i].spans[j].startIndex; k <= texts[i].spans[j].endIndex; k++) {
                    attributes[k] = j;
                }
            }
        }
        for(int j = startIdx; j < lines.count; j++) {
            blurg_measure_line(blurg, &texts[i], attributes, &lines, breaks, j, maxWidth);
            if(lines.data[j].width > alignWidth) {
                alignWidth = lines.data[j].width;
            }
            if(lines.data[j].width > w) {
                w = lines.data[j].width;
            }
            h += lines.data[j].lineHeight;
        }
        if(attributes)
            free(attributes);
        free(breaks);
    }
    
    // adjust for alignment
    if(hasAlign && width) {
        for(int i = 0; i < lines.count; i++) {
            int pIdx = lines.data[i].paraIndex;
            if(lines.data[i].isBreak ||
               texts[pIdx].alignment == blurg_align_left) {
               continue;
            }
            float offsetW = 0;
            if(texts[pIdx].alignment == blurg_align_right) {
                offsetW = (alignWidth - lines.data[i].width);
            }
            if(texts[pIdx].alignment == blurg_align_center) {
                offsetW = (alignWidth / 2.0) - (lines.data[i].width / 2.0);
            }
            if(offsetW + lines.data[i].width > w) {
                w = offsetW + lines.data[i].width;
            }
        }
    }

    list_text_line_free(&lines);
    if(width) {
        *width = w;
    }
    if(height) {
        *height = h;
    }
}

BLURGAPI void blurg_measure_string(blurg_t *blurg, blurg_font_t *font, float size, const char *text, float *width, float *height)
{
    blurg_formatted_text_t formatted = {
        .defaultFont = font,
        .defaultSize = size,
        .defaultColor = 0xFFFFFFFF,
        .encoding = blurg_encoding_utf8,
        .spanCount = 0,
        .spans = NULL,
        .text = text,
        .alignment = blurg_align_left,
    };
    return blurg_measure_formatted(blurg, &formatted, 1, 0, width, height);
}

BLURGAPI void blurg_measure_string_utf16(blurg_t *blurg, blurg_font_t *font, float size, const uint16_t *text, float *width, float *height)
{
    blurg_formatted_text_t formatted = {
        .defaultFont = font,
        .defaultSize = size,
        .defaultColor = 0xFFFFFFFF,
        .encoding = blurg_encoding_utf16,
        .spanCount = 0,
        .spans = NULL,
        .text = text,
        .alignment = blurg_align_left,
    };
    return blurg_measure_formatted(blurg, &formatted, 1, 0, width, height);
}

BLURGAPI blurg_rect_t* blurg_build_string(blurg_t *blurg, blurg_font_t *font, float size, blurg_color_t color, const char *text, int* rectCount, float *width, float *height)
{
    blurg_formatted_text_t formatted = {
        .defaultFont = font,
        .defaultSize = size,
        .defaultColor = color,
        .defaultShadow = BLURG_NO_SHADOW,
        .defaultUnderline = BLURG_NO_UNDERLINE,
        .encoding = blurg_encoding_utf8,
        .spanCount = 0,
        .spans = NULL,
        .text = text,
        .alignment = blurg_align_left,
    };
    blurg_rect_t *retval = blurg_build_formatted(blurg, &formatted, 1, 0, rectCount, width, height);
    return retval;
}

BLURGAPI blurg_rect_t* blurg_build_string_utf16(blurg_t *blurg, blurg_font_t *font, float size, blurg_color_t color, const uint16_t *text, int* rectCount, float *width, float *height)
{
    blurg_formatted_text_t formatted = {
        .defaultFont = font,
        .defaultSize = size,
        .defaultColor = color,
        .defaultShadow = BLURG_NO_SHADOW,
        .defaultUnderline = BLURG_NO_UNDERLINE,
        .encoding = blurg_encoding_utf16,
        .spanCount = 0,
        .spans = NULL,
        .text = text,
        .alignment = blurg_align_left,
    };
    blurg_rect_t *retval = blurg_build_formatted(blurg, &formatted, 1, 0, rectCount, width, height);
    return retval;
}

BLURGAPI void blurg_free_rects(blurg_rect_t *rects)
{
    free(rects);
}

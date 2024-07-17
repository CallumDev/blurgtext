#include <raqm.h>
#include "blurgtext_internal.h"
#include <string.h>
#include <linebreak.h>
#include "list.h"
#include <wchar.h>
#include <math.h>

DEFINE_LIST(blurg_rect_t);
IMPLEMENT_LIST(blurg_rect_t);

typedef struct {
    int isBreak;
    int textStart;
    int textCount;
    float width;
    float lineHeight;
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
    return blurg;
}

BLURGAPI void blurg_destroy(blurg_t *blurg)
{
    glyphatlas_destroy(blurg);
    FT_Done_Library(blurg->library);
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

static void blurg_get_lines(const void *text, int* textLen, blurg_encoding_t encoding, list_text_line *lines, char **breaks)
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
                list_text_line_add(lines, (text_line){ .isBreak = 1, .textStart = i, .textCount = 1});
            } else {
                list_text_line_add(lines, (text_line){ .isBreak = 0, .textStart = last, .textCount = i - last});
            }
            last = i + 1;
        }
    }
    if(total - last > 0) {
        list_text_line_add(lines, (text_line){.isBreak = 0, .textStart = last, .textCount = total - last });
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

static void add_underline(active_underline ul, float xEnd, list_blurg_rect_t *rb, float y)
{
    list_blurg_rect_t_add(rb, (blurg_rect_t){
        .texture = rb->data[rb->count - 1].texture,
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
                    add_underline(ul, shadowX, rb, shadowY);
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
                add_underline(ul, shadowX, rb, shadowY);
                ul.active = 0;
            }
            else if (ul.active && underline.enabled && (
                shadow.color != ul.color ||
                shadow.pixels != ul.offset ||
                upos != ul.pos)) {
                add_underline(ul, shadowX, rb, shadowY);
                ul.xStart = shadowX;
                ul.color = shadow.color;
                ul.offset = shadow.pixels;
            }
            shadowX += glyphs[i].x_advance / 64.0;
            shadowY += glyphs[i].y_advance / 64.0;
        }
        if(ul.active) {
            add_underline(ul, shadowX, rb, *y);
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
                add_underline(ul, underlineX, rb, underlineY);
                ul.active = 0;
            }
            else if (ul.active && underline.enabled && (
                ucolor != ul.color ||
                upos != ul.pos)) {
                add_underline(ul, underlineX, rb, underlineY);
                ul.xStart = *x;
                ul.color = ucolor;
                ul.offset = 0;
                ul.pos = upos;
            }
            underlineX += glyphs[i].x_advance / 64.0;
            underlineY += glyphs[i].y_advance / 64.0;
        }
        if(ul.active) {
            add_underline(ul, underlineX, rb, underlineY);
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

// returns the length of text shaped/turned into rects for current maxWidth
static int blurg_shape_multiple(blurg_t *blurg, const void *str, char *breaks, int len, float size,
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
    if(maxWidth > 0) {
        // wrap text if necessary
        raqm_glyph_t *glyphs = raqm_get_glyphs (rq, &count);
        float cx = *x;
        size_t stop;
        for(stop = 0; stop < count; stop++) {
            float advance = (glyphs[stop].x_advance / 64.0);
            if(cx + advance > maxWidth) {
                break;
            }
            cx += advance;
        }
        // Text is too big, we need to line break!
        if(stop != count) {
            if(*x <= 0 && stop == 0) { 
                // too big to put a single character on a line?
                // just output one glyph
                stop = 1;
                count = 1;
                charCount = (int)glyphs[1].cluster;
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
                    if(found && (stop + 1) < count) {
                        //skip the space we converted into a break
                        charCount = (int)glyphs[stop + 1].cluster;
                    } else {
                        charCount = (int)glyphs[stop].cluster;
                    }
                    count = stop;
                } else {
                    //no appropriate line break, just insert an \n
                    count = stop;
                    charCount = (int)glyphs[stop].cluster;
                }
            }
        }
    }
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
        .textCount = newCount
    };
    list_text_line_insert(lines, lineIndex + 1, newLine);
}

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
        return;
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

    // accessors for attribute and text arrays
    #define TEXT_OFFSET(x) (text->encoding == blurg_encoding_utf16 ? (const void*)(\
        &((const utf16_t*)(text->text))[(x)] \
    ) : (const void*) (\
        &((const char*)(text->text))[(x)] \
    ))
    #define ATTRIBUTE_OFFSET(x) (attributes ? &attributes[(x)] : NULL)

    for(int i = 0; i < count; i++) {
        blurg_font_t *fontAtIndex = IDX_FONT(start + i);
        float sizeAtIndex = IDX_SIZE(start + i);
        // size change, we need to shape text
        if(currentSize != sizeAtIndex) {
            if(i - last > 0) {
                int shapeCount = i - last;
                int actualCount = blurg_shape_multiple(
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
        int actualCount = blurg_shape_multiple(
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
    #undef TEXT_OFFSET
    #undef ATTRIBUTE_OFFSET

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

BLURGAPI blurg_rect_t* blurg_build_formatted(blurg_t *blurg, blurg_formatted_text_t *text, float maxWidth, int *rectCount)
{
    int total;
    list_text_line lines;
    list_text_line_init(&lines, 8);
    char *breaks;
    blurg_get_lines(text->text, &total, text->encoding, &lines, &breaks);
    if(total == 0) {
        *rectCount = 0;
        list_text_line_free(&lines);
        free(breaks);
        return NULL;
    }

    // Attribute lookup table
    int* attributes = NULL;
    if(text->spans && text->spanCount) {
        attributes = malloc(total * sizeof(int));
        for(int i = 0; i < total; i++) 
        {
            attributes[i] = -1;
        }
        for(int i = 0; i < text->spanCount; i++) {
            for(int j = text->spans[i].startIndex; j <= text->spans[i].endIndex; j++) {
                attributes[j] = i;
            }
        }
    }

    float alignWidth = 0;
    // non-left aligned, shape all lines before copying to buffer
    if(text->alignment != blurg_align_left) {
        for(int i = 0; i < lines.count; i++) {
            blurg_wrap_shape_line(blurg, text, attributes, &lines, breaks, i, maxWidth);
            if(lines.data[i].width > alignWidth) {
                alignWidth = lines.data[i].width;
            }
        }
    }

    // lines to shaped text
    list_blurg_rect_t rects;
    list_blurg_rect_t_init(&rects, total);
    float y = 0;
    for(int i = 0; i < lines.count; i++) {
        if(text->alignment == blurg_align_left) //we don't need to allocate line buffers upfront if left aligned
            blurg_wrap_shape_line(blurg, text, attributes, &lines, breaks, i, maxWidth);
        if(!lines.data[i].isBreak) {
            // copy rects
            for(int j = 0; j < lines.data[i].rects.count; j++) {
                //process alignment
                if(text->alignment == blurg_align_right) {
                    lines.data[i].rects.data[j].x += (alignWidth - lines.data[i].width);
                }
                if(text->alignment == blurg_align_center) {
                    lines.data[i].rects.data[j].x += (alignWidth / 2.0) - (lines.data[i].width / 2.0);
                }
                //position line
                lines.data[i].rects.data[j].y += y;
            }
            list_blurg_rect_t_add_range(&rects, &lines.data[i].rects);
            list_blurg_rect_t_free(&lines.data[i].rects);
        }
        y += lines.data[i].lineHeight;
    }
    //cleanup and return
    if(attributes)
        free(attributes);
    free(breaks);
    list_text_line_free(&lines);
    
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


BLURGAPI blurg_rect_t* blurg_build_string(blurg_t *blurg, blurg_font_t *font, float size, blurg_color_t color, const char *text, int* rectCount)
{
    blurg_formatted_text_t formatted = {
        .defaultFont = font,
        .defaultSize = size,
        .defaultColor = color,
        .encoding = blurg_encoding_utf8,
        .spanCount = 0,
        .spans = NULL,
        .text = text,
        .alignment = blurg_align_left,
    };
    return blurg_build_formatted(blurg, &formatted, 0, rectCount);
}

BLURGAPI void blurg_free_rects(blurg_rect_t *rects)
{
    free(rects);
}

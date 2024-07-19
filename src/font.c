#include "blurgtext_internal.h"
#include <string.h>
#include <ctype.h>
#include "util.h"

#define DPI 72

static uint32_t fnv1a_str(char *str)
{
    unsigned char *s = (unsigned char *)str;	/* unsigned string */
    uint32_t hval = 0x811c9dc5;
    while (*s) {
        hval ^= (uint32_t)*s++;
	    hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
    }
    return hval;
}

// unrolled FNV1A face hash loop based on size
static uint32_t fnv1a_combined(uint32_t faceHash, uint32_t size)
{
    uint32_t hval = faceHash;
    hval ^= (size & 0xFF);
    hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
    hval ^= ((size >> 8) & 0xFF);
	hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
    hval ^= ((size >> 16) & 0xFF);
	hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
    hval ^= ((size >> 24) & 0xFF);
	hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
    return hval;
}

// Fonts are freed when FT_Done_Library is called in the main destroy
static void font_finalizer(void* object)
{
    FT_Face face = (FT_Face)object;
    free(face->generic.data);
}

blurg_font_t *blurg_from_freetype(FT_Face face)
{
    if(face->generic.finalizer != font_finalizer) {
        blurg_font_t *fnt = malloc(sizeof(blurg_font_t));
        memset(fnt, 0, sizeof(blurg_font_t));
        char hashbuffer[2048];
        snprintf(
            hashbuffer, 2048, "%i;%s;%s", 
            face->face_index, 
            face->style_name, 
            face->family_name
        );
        fnt->faceHash = fnv1a_str(hashbuffer);
        fnt->face = face;
        face->generic.data = fnt;
        face->generic.finalizer = font_finalizer;
        return fnt;
    }
    return (blurg_font_t*)face->generic.data;
}

void font_use_size(blurg_font_t *fnt, float size)
{
    int sizeVal = (int)(size * 64.0);
    if(fnt->setSize == sizeVal)
        return;

    FT_Face face = fnt->face;
    FT_Set_Char_Size(face, 0, sizeVal, DPI, DPI);
    // metrics
    fnt->ascender = face->size->metrics.ascender / 64.0;
    float descent = face->size->metrics.descender / 64.0;
    float height = (face->size->metrics.height / 64.0);
    float linegap = height - fnt->ascender + descent;
    fnt->lineHeight = fnt->ascender - descent + linegap;
    // hash
    fnt->setSize = sizeVal;
    fnt->hash = fnv1a_combined(fnt->faceHash, (uint32_t)sizeVal);
}

static void SetCharmap(FT_Face face)
{
    FT_CharMap found = NULL;
    for (int i = 0; i < face->num_charmaps; i++) {
        FT_CharMap charmap = face->charmaps[i];
        if (charmap->platform_id == 3 && charmap->encoding_id == 10) { /* UCS-4 Unicode */
            found = charmap;
            break;
        }
    }
    if (!found) {
        for (int i = 0; i < face->num_charmaps; i++) {
            FT_CharMap charmap = face->charmaps[i];
            if ((charmap->platform_id == 3 && charmap->encoding_id == 1) /* Windows Unicode */
             || (charmap->platform_id == 3 && charmap->encoding_id == 0) /* Windows Symbol */
             || (charmap->platform_id == 2 && charmap->encoding_id == 1) /* ISO Unicode */
             || (charmap->platform_id == 0)) { /* Apple Unicode */
                found = charmap;
                break;
            }
        }
    }
    if (found) {
        /* If this fails, continue using the default charmap */
        FT_Set_Charmap(face, found);
    } 
}

static void get_face_information(FT_Face face, int* weight, int* italic)
{
    *italic = (face->style_flags & FT_STYLE_FLAG_ITALIC) == FT_STYLE_FLAG_ITALIC;
    *weight = BLURG_WEIGHT_REGULAR;
    if(face->style_name) {
        int len;
        char *lw = strlower(face->style_name, &len);
        if(len >= 10 && (
            strstr(lw, "extrablack") || 
            strstr(lw, "ultrablack") ||
            strstr(lw, "extra black") ||
            strstr(lw, "ultrablack")
        )) {
            *weight = BLURG_WEIGHT_EXTRABOLD;
        }
        else if (len >= 10 && (
            strstr(lw, "extralight") ||
            strstr(lw, "ultralight") ||
            strstr(lw, "extra light") ||
            strstr(lw, "ultra light")
        )) {
            *weight = BLURG_WEIGHT_EXTRALIGHT;
        }
        else if (len >= 9 && (
            strstr(lw, "demilight") ||
            strstr(lw, "demi light")
        )) {
            *weight = BLURG_WEIGHT_LIGHT;
        }
        else if (len >= 8 && (
            strstr(lw, "demibold") ||
            strstr(lw, "semibold") ||
            strstr(lw, "demi bold") ||
            strstr(lw, "demibold")
        )) {
            *weight = BLURG_WEIGHT_SEMIBOLD;
        }
        else if (len >= 6 && strstr(lw, "medium")) {
            *weight = BLURG_WEIGHT_MEDIUM;
        }
        else if (len >= 5 && (strstr(lw,"black") || strstr(lw, "heavy"))) {
            *weight = BLURG_WEIGHT_BLACK;
        }
        else if (
            strstr(lw, "regular") ||
            strstr(lw, "normal") ||
            strstr(lw, "regular") ||
            strstr(lw,"text")
        ) {
            *weight = BLURG_WEIGHT_REGULAR;
        }
        else if (strstr(lw, "thin")) {
            *weight = BLURG_WEIGHT_THIN;
        }
        else if (strstr(lw, "bold")) {
            *weight = BLURG_WEIGHT_BOLD;
        }
        free(lw);
    }
    if((face->style_flags & FT_STYLE_FLAG_BOLD) == FT_STYLE_FLAG_BOLD)
    {
        if(*weight <= BLURG_WEIGHT_REGULAR)
            *weight = BLURG_WEIGHT_BOLD;
    }
}

BLURGAPI const char *blurg_font_get_family(blurg_font_t *font)
{
    return font->face->family_name;
}

BLURGAPI int blurg_font_get_italic(blurg_font_t *font)
{
    return font->italic;
}

BLURGAPI int blurg_font_get_weight(blurg_font_t *font)
{
    return font->weight;
}

BLURGAPI float blurg_font_get_line_height(blurg_font_t *font, float size)
{
    font_use_size(font, size);
    return font->lineHeight;
}

blurg_font_t *blurg_font_create_internal(blurg_t *blurg, allocated_font *data)
{
    FT_Face face;
    FT_Error error = FT_New_Memory_Face(blurg->library, data->data, data->dataLen, 0, &face);

    if(error) {
        printf("FT_New_Memory_Face failed: %s\n", FT_Error_String(error));
        return NULL;
    }
    SetCharmap(face);
    blurg_font_t *font = blurg_from_freetype(face);
    get_face_information(face, &font->weight, &font->italic);
    return font;
}

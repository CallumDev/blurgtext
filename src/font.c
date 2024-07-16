#include "blurgtext_internal.h"
#include <hb.h>
#include <hb-ft.h>
#include <string.h>

#define DPI 72

static hb_user_data_key_t blurg_key;

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

blurg_font_t *blurg_from_freetype(FT_Face face)
{
    hb_face_t* hb = hb_ft_face_create_cached(face);
    void* d = hb_face_get_user_data(hb, &blurg_key);
    if(!d) {
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
        hb_face_set_user_data(hb, &blurg_key, fnt, NULL, true);
        return fnt;
    }
    return (blurg_font_t*)d;
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

BLURGAPI blurg_font_t *blurg_font_create(blurg_t *blurg, const char *filename)
{
    FT_Face face;
    FT_Error error = FT_New_Face(blurg->library, filename, 0, &face);
    if(error) {
        printf("FT_New_face failed: %s\n", FT_Error_String(error));
        return NULL;
    }
    SetCharmap(face);
    blurg_font_t *font = blurg_from_freetype(face);
    return font;
}


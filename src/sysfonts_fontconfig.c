#if BT_ENABLE_FONTCONFIG
#include "blurgtext_internal.h"
#include <fontconfig/fontconfig.h>
#include FT_COLOR_H
typedef struct {
    FcConfig* fc;
} sysfc;

BLURGAPI int blurg_enable_system_fonts(blurg_t *blurg)
{
    if(blurg->sysFontData) {
        return 1;
    }
    if(!FcInit()) {
        return 0;
    }
    sysfc *ctx = malloc(sizeof(sysfc));
    blurg->sysFontData = ctx;
    ctx->fc = FcInitLoadConfigAndFonts();
    return 1;
}

static int check_character(FcPattern *font, uint32_t character)
{
    if(!character)
        return 1;
    FcCharSet* cs;
    for (int index = 0; ; index++) {
        FcResult result = FcPatternGetCharSet(font, FC_CHARSET, index, &cs);
        if (result == FcResultNoId) {
            break;
        }
        if (result == FcResultMatch && FcCharSetHasChar(cs, character)) {
            return 1;
        }
    }
    return 0;
}

blurg_font_t *blurg_sysfonts_query(blurg_t *blurg, const char *familyName, int weight, int italic, uint32_t character)
{
    if(!blurg->sysFontData) {
        return NULL;
    }
    sysfc *ctx = blurg->sysFontData;
    FcPattern *pat = FcPatternCreate();
    FcPatternAddString(pat, FC_FAMILY, familyName);
    FcPatternAddInteger(pat, FC_SLANT, italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
    FcPatternAddInteger(pat, FC_WEIGHT, FcWeightFromOpenType(weight));

    FcCharSet *cs = NULL;
    if(character) {
        cs = FcCharSetCreate();
        FcCharSetAddChar(cs, character);
        FcPatternAddCharSet(pat, FC_CHARSET, cs);
    }
    FcConfigSubstitute(ctx->fc, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    blurg_font_t *bfnt = NULL;
    FcResult result;
    FcPattern *font = FcFontMatch(ctx->fc, pat, &result);

    if(font) {
        FcChar8 *file = NULL;
        FcBool embolden;
        if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch &&
            check_character(font, character))
        {
            bfnt = blurg_font_add_file(blurg, file);
            if(bfnt) {
                if((FcPatternGetBool(font, FC_EMBOLDEN, 0, &embolden) == FcResultMatch) && embolden) {
                    bfnt->embolden = 1;
                    blurg_font_rehash(bfnt);                
                }
                int weight = 0;
            }
        }
    } 
    FcPatternDestroy(font);
    FcPatternDestroy(pat);
    FcCharSetDestroy(cs);
    return bfnt;
}

void blurg_sysfonts_destroy(blurg_t *blurg)
{
    if(!blurg->sysFontData) {
        return;
    }
    sysfc *ctx = blurg->sysFontData;
    FcConfigDestroy(ctx->fc);
    free(ctx);
}
#endif
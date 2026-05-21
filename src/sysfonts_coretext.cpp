#if BT_ENABLE_CORETEXT
#include "blurgtext_internal.h"
#include "util.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <math.h>
#include <string.h>

#include <memory>
#include <type_traits>

struct CFSafeRelease 
{
    void operator()(CFTypeRef cfTypeRef) 
    {
      if (cfTypeRef) 
      {
          CFRelease(cfTypeRef);
      }
    }
};

template <typename CFRef>
struct CFObj {
    std::unique_ptr<std::remove_pointer_t<CFRef>, CFSafeRelease> ptr;

    CFObj() = default;
    explicit CFObj(CFRef value) : ptr(value) {}

    operator CFRef() const { return ptr.get(); }
    CFRef get() const { return ptr.get(); }
    explicit operator bool() const { return ptr != nullptr; }
    explicit operator void*() const { return ptr.get(); }
};


CFStringRef str_from_type(CFTypeRef typeRef)
{
    if(typeRef && CFGetTypeID(typeRef) == CFStringGetTypeID()) 
    {
        return (CFStringRef)typeRef;
    }
    return NULL;
}

CFDictionaryRef dict_from_type(CFTypeRef typeRef)
{
    if(typeRef && CFGetTypeID(typeRef) == CFDictionaryGetTypeID()) 
    {
        return (CFDictionaryRef)typeRef;
    }
    return NULL;
}

// Font Weight Conversion


struct WeightMapping
{
    int blurg_weight;
    double ct_weight;
};

static double map_weight_to_ct(
    int src_value,
    int src_min,
    int src_max,
    double dst_min,
    double dst_max)
{
    if(src_max <= src_min) {
        return dst_min;
    }

    return dst_min +
           (((double)(src_value - src_min) * (dst_max - dst_min)) /
            (double)(src_max - src_min));
}

static int map_weight_from_ct(
    double src_value,
    double src_min,
    double src_max,
    int dst_min,
    int dst_max)
{
    if(src_max <= src_min) {
        return dst_min;
    }

    return (int)(
        dst_min +
        (((src_value - src_min) * (double)(dst_max - dst_min)) /
         (src_max - src_min)) +
        0.5
    );
}

static const WeightMapping allWeights[] = {
    { 0,                        -1.0 },
    { BLURG_WEIGHT_THIN,        -0.8 },
    { BLURG_WEIGHT_EXTRALIGHT,  -0.6 },
    { BLURG_WEIGHT_LIGHT,       -0.4 },
    { BLURG_WEIGHT_REGULAR,      0.0 },
    { BLURG_WEIGHT_MEDIUM,       0.23 },
    { BLURG_WEIGHT_SEMIBOLD,     0.3 },
    { BLURG_WEIGHT_BOLD,         0.4 },
    { BLURG_WEIGHT_EXTRABOLD,    0.56 },
    { BLURG_WEIGHT_BLACK,        0.62 },
    { 1000,                      1.0 },
};
static int weightCount = sizeof(allWeights) / sizeof(allWeights[0]);

static double weight_to_ct(int weight)
{
    if(weight <= allWeights[0].blurg_weight) 
    {
        return allWeights[0].ct_weight;
    }

    for(size_t i = 0; i + 1 < weightCount; ++i) 
    {
        if(weight <= allWeights[i + 1].blurg_weight) 
        {
            return map_weight_to_ct(
                weight,
                allWeights[i].blurg_weight,
                allWeights[i + 1].blurg_weight,
                allWeights[i].ct_weight,
                allWeights[i + 1].ct_weight
            );
        }
    }
    return allWeights[weightCount - 1].ct_weight;
}

static int weight_from_ct(double weight)
{
    if(weight <= allWeights[0].ct_weight) 
    {
        return allWeights[0].blurg_weight;
    }

    for(size_t i = 0; i + 1 < weightCount; ++i) 
    {
        if(weight <= allWeights[i + 1].ct_weight) 
        {
            return map_weight_from_ct(
                weight,
                allWeights[i].ct_weight,
                allWeights[i + 1].ct_weight,
                allWeights[i].blurg_weight,
                allWeights[i + 1].blurg_weight
            );
        }
    }
    return allWeights[weightCount - 1].blurg_weight;
}


// Reference: https://gist.github.com/paxbun/74054e88d0a5c9dfe68a6a0860a28f58

struct FontHeader 
{
    int32_t fVersion;
    uint16_t fNumTables;
    uint16_t fSearchRange;
    uint16_t fEntrySelector;
    uint16_t fRangeShift;
};

struct TableEntry {
    uint32_t fTag;
    uint32_t fCheckSum;
    uint32_t fOffset;
    uint32_t fLength;
};

static uint32_t table_checksum(const uint32_t *table, uint32_t tableByteLength) 
{
    uint32_t sum = 0;
    uint32_t nLongs = (tableByteLength + 3) / 4;
    while (nLongs-- > 0) 
    {
       sum += CFSwapInt32HostToBig(*table++);
    }
    return sum;
}

static unsigned char *ttf_from_cgfont(CGFontRef fontPtr, size_t *length)
{
    if(!fontPtr)
    {
        return nullptr;
    }
    CFRetain(fontPtr);

    CFObj<CGFontRef> font(fontPtr);
 
    CFArrayRef tags = CGFontCopyTableTags(font);
    int tableCount = CFArrayGetCount(tags);

    size_t *tableSizes = (size_t*)malloc(sizeof(size_t) * tableCount);
    memset(tableSizes, 0, sizeof(size_t) * tableCount);
    
    bool containsCFFTable = false;
    
    size_t totalSize = sizeof(FontHeader) + sizeof(TableEntry) * tableCount;
    
    for (int index = 0; index < tableCount; index++) 
    {
        //get size
        size_t tableSize = 0;
        uint32_t aTag = (uint32_t)(uintptr_t)CFArrayGetValueAtIndex(tags, index);
       
 		if (aTag == 'CFF ' && !containsCFFTable) 
        {
            containsCFFTable = true;
        }
        
        CFDataRef tableDataRef = CGFontCopyTableForTag(font, aTag);
        if (tableDataRef != NULL) {
            tableSize = CFDataGetLength(tableDataRef);
            CFRelease(tableDataRef);
        }
        totalSize += (tableSize + 3) & ~3;
        
        tableSizes[index] = tableSize;
    }

    unsigned char *stream = (unsigned char*)malloc(totalSize);
    
    memset(stream, 0, totalSize);
    char* dataStart = (char*)stream;
    char* dataPtr = dataStart;

    // compute font header entries
    uint16_t entrySelector = 0;
    uint16_t searchRange = 1;
    
    while (searchRange < tableCount >> 1) 
    {
        entrySelector++;
        searchRange <<= 1;
    }
    searchRange <<= 4;

    uint16_t rangeShift = (tableCount << 4) - searchRange;

    // write font header (also called sfnt header, offset subtable)
    FontHeader* offsetTable = (FontHeader*)dataPtr;
       
	//OpenType Font contains CFF Table use 'OTTO' as version, and with .otf extension
	//otherwise 0001 0000
    offsetTable->fVersion = containsCFFTable ? 'OTTO' : CFSwapInt16HostToBig(1);
    offsetTable->fNumTables = CFSwapInt16HostToBig((uint16_t)tableCount);
    offsetTable->fSearchRange = CFSwapInt16HostToBig((uint16_t)searchRange);
    offsetTable->fEntrySelector = CFSwapInt16HostToBig((uint16_t)entrySelector);
    offsetTable->fRangeShift = CFSwapInt16HostToBig((uint16_t)rangeShift);

    dataPtr += sizeof(FontHeader);

    // write tables
    TableEntry* entry = (TableEntry*)dataPtr;
    dataPtr += sizeof(TableEntry) * tableCount;
    
    for (int index = 0; index < tableCount; ++index) 
    {
        uint32_t aTag = (uint32_t)(uintptr_t)CFArrayGetValueAtIndex(tags, index);
        CFDataRef tableDataRef = CGFontCopyTableForTag(font, aTag);
        size_t tableSize = CFDataGetLength(tableDataRef);
        
        memcpy(dataPtr, CFDataGetBytePtr(tableDataRef), tableSize);
        
        entry->fTag = CFSwapInt32HostToBig((uint32_t)aTag);
        entry->fCheckSum = CFSwapInt32HostToBig(table_checksum((uint32_t *)dataPtr, tableSize));
        
        uint32_t offset = dataPtr - dataStart;
        entry->fOffset = CFSwapInt32HostToBig((uint32_t)offset);
        entry->fLength = CFSwapInt32HostToBig((uint32_t)tableSize);
        dataPtr += (tableSize + 3) & ~3;
        entry++;
        CFRelease(tableDataRef);
    }

    free(tableSizes);

    *length = (size_t)(dataPtr - dataStart);

    return stream;
}


static void get_font_traits(CTFontRef font, int *weight, int *italic)
{
    double ct_weight = 0.0;
    CFObj<CFDictionaryRef> traits(dict_from_type(CTFontCopyTraits(font)));

    if(traits) 
    {
        CFNumberRef weight_ref = (CFNumberRef)CFDictionaryGetValue(traits, kCTFontWeightTrait);
        if(weight_ref) 
        {
            CFNumberGetValue(weight_ref, kCFNumberDoubleType, &ct_weight);
        }
    }

    *weight = weight_from_ct(ct_weight);
    *italic = (CTFontGetSymbolicTraits(font) & kCTFontItalicTrait) != 0;
}

static blurg_font_t *load_ctfont(blurg_t *blurg, CTFontRef fontPtr, int wantBold)
{
    CFRetain(fontPtr);
    CFObj<CTFontRef> font(fontPtr);
    CGFontRef cgfont = CTFontCopyGraphicsFont(font, NULL);

    if(!cgfont) 
    {
        return NULL;
    }

    size_t fontDataLen = 0;
    unsigned char *fontData = ttf_from_cgfont(cgfont, &fontDataLen);
    CGFontRelease(cgfont);
    if(!fontData) 
    {
        return NULL;
    }

    blurg_font_t *resolved = blurg_font_add_memory(blurg, (char*)fontData, (int)fontDataLen, 1);
    free(fontData);
    if(!resolved) 
    {
        return NULL;
    }

    int resolved_weight = 0;
    int resolved_italic = 0;

    get_font_traits(font, &resolved_weight, &resolved_italic);
    if(wantBold && resolved_weight <= BLURG_WEIGHT_REGULAR)
    {
        resolved->embolden = 1;
        blurg_font_rehash(resolved);
    }
    // todo: italic test
    return resolved;
}

// Matching


static int family_matches(CTFontDescriptorRef descriptor, CFStringRef family)
{
    CFObj<CFStringRef> matched_family(str_from_type(CTFontDescriptorCopyAttribute(descriptor, kCTFontFamilyNameAttribute)));
    int matches = 0;

    if(matched_family) 
    {
        matches = CFStringCompare(matched_family,family,kCFCompareCaseInsensitive);
    }

    return matches == kCFCompareEqualTo;
}

static void get_descriptor_traits(
    CTFontDescriptorRef descriptor,
    double *weight,
    int *italic)
{
    *weight = 0.0;
    *italic = 0;
    CFObj<CFDictionaryRef> traits(dict_from_type(CTFontDescriptorCopyAttribute(descriptor, kCTFontTraitsAttribute)));
    if(traits)
    {
        CFNumberRef weight_ref = (CFNumberRef)CFDictionaryGetValue(traits, kCTFontWeightTrait);
        if(weight_ref) 
        {
            CFNumberGetValue(weight_ref, kCFNumberDoubleType, weight);
        }
        CFNumberRef symbolic_ref =(CFNumberRef)CFDictionaryGetValue(traits, kCTFontSymbolicTrait);
        if(symbolic_ref)
        {
            uint32_t symbolic_traits = 0;
            CFNumberGetValue(symbolic_ref, kCFNumberSInt32Type, &symbolic_traits);
            *italic = (symbolic_traits & kCTFontItalicTrait) != 0;
        }
    }
}

static CTFontDescriptorRef select_descriptor(
    const char *family_name,
    int weight,
    int italic)
{
    const void *trait_keys[] = {
        kCTFontWeightTrait,
        kCTFontSymbolicTrait
    };
    const void *attribute_keys[] = {
        kCTFontFamilyNameAttribute,
        kCTFontTraitsAttribute
    };
    const void *trait_values[2];
    const void *attribute_values[2];

    double ct_weight = weight_to_ct(weight);
    uint32_t symbolic_traits = italic ? kCTFontItalicTrait : 0;

    CFObj<CFStringRef> family(CFStringCreateWithCString(kCFAllocatorDefault,family_name,kCFStringEncodingUTF8));
    if(!family) 
    {
        return NULL;
    }

    CFObj<CFNumberRef> weight_ref(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &ct_weight));
    CFObj<CFNumberRef> symbolic_ref(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &symbolic_traits));
    
    if(!weight_ref || !symbolic_ref)
    {
        return NULL;
    }

    trait_values[0] = weight_ref;
    trait_values[1] = symbolic_ref;

    CFObj<CFDictionaryRef> traits(CFDictionaryCreate(
        kCFAllocatorDefault,
        trait_keys,
        trait_values,
        2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    ));

    if(!traits) 
    {
        return NULL;
    }

    attribute_values[0] = family;
    attribute_values[1] = traits;

    CFObj<CFDictionaryRef> attributes(CFDictionaryCreate(
        kCFAllocatorDefault,
        attribute_keys,
        attribute_values,
        2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    ));

    if(!attributes) 
    {
        return NULL;
    }

    CFObj<CTFontDescriptorRef> query(CTFontDescriptorCreateWithAttributes(attributes));
    if(!query) 
    {
        return NULL;
    }

    CTFontDescriptorRef best = NULL;

    CFObj<CFArrayRef> matches(CTFontDescriptorCreateMatchingFontDescriptors(query, NULL));
    if(matches) {
        double best_score = HUGE_VAL;
        CFIndex match_count = CFArrayGetCount(matches);

        for(CFIndex i = 0; i < match_count; ++i) {
            CTFontDescriptorRef candidate = (CTFontDescriptorRef)CFArrayGetValueAtIndex(matches, i);

            if(!candidate || !family_matches(candidate, family)) {
                continue;
            }

            double candidate_weight = 0.0;
            int candidate_italic = 0;
            get_descriptor_traits(candidate, &candidate_weight, &candidate_italic);

            double score =
                fabs(candidate_weight - ct_weight) +
                (candidate_italic == italic ? 0.0 : 2.0);

            if(score < best_score) {
                best_score = score;
                best = candidate;
            }
        }
    }

    if(best) 
    {
        CFRetain(best);
    }
    return best;
}

blurg_font_t *blurg_sysfonts_query(
    blurg_t *blurg,
    const char *family_name,
    int weight,
    int italic,
    uint32_t character)
{
    if(!blurg->sysFontData) 
    {
        return NULL;
    }

    CFObj<CTFontDescriptorRef> descriptor(select_descriptor(family_name, weight, italic));
    CFObj<CTFontRef> base_font(CTFontCreateWithFontDescriptor(descriptor, 12.0, NULL));

    if(!base_font) 
    {
        return NULL;
    }

    if(character) {
        uint16_t utf16[2];
        CGGlyph glyphs[2] = { 0, 0 };
        uint32_t utf16_len = utf32_to_utf16(character, utf16);

        if(!CTFontGetGlyphsForCharacters(base_font, utf16, glyphs, utf16_len) ||
           !glyphs[0])
        {
            CFObj<CFStringRef> str (CFStringCreateWithCharacters(kCFAllocatorDefault, utf16, utf16_len));
            if(!str) 
            {
                return NULL;
            }

            CFObj<CTFontRef> fallback (CTFontCreateForString(base_font, str, CFRangeMake(0, CFStringGetLength(str))));

            if(!fallback) 
            {
                return NULL;
            }

            return load_ctfont(blurg, fallback, weight >= BLURG_WEIGHT_BOLD);
        }
    }

    return load_ctfont(blurg, base_font, weight >= BLURG_WEIGHT_BOLD);
}


BLURGAPI int blurg_enable_system_fonts(blurg_t *blurg)
{
    /* CoreText requires no saved data */
    if(blurg->sysFontData) 
    {
        return 1;
    }
    blurg->sysFontData = (void*)1;
    return 1;
}

void blurg_sysfonts_destroy(blurg_t *blurg)
{
    blurg->sysFontData = NULL;
}


#endif

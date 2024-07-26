#include "blurgtext_internal.h"
#include <ctype.h>
#include "util.h"

typedef struct _font_lookup_node {
    uint32_t key;
    int nextIndex;
    blurg_font_t *font;
} font_lookup_node;

DEFINE_LIST(font_lookup_node)
IMPLEMENT_LIST(font_lookup_node)

struct _font_manager {
    struct hashmap *fontTable;
    struct hashmap *fileTable;
    const char *defaultFont;
    list_font_lookup_node nodes;
};

typedef struct _font_data_entry {
    char *filename;
    allocated_font *font;
} font_data_entry;

uint64_t case_insensitive_hash(const char *item, uint64_t seed0, uint64_t seed1)
{
    size_t len = strlen(item);
    if(len > 1024) 
        len = 1024;
    char buf[1024];
    for(int i = 0; i < len; i++) {
        buf[i] = tolower(item[i]);
    }
    return hashmap_sip(buf, len, seed0, seed1);
}

int font_data_entry_compare(const void *a, const void *b, void *udata)
{
    const font_data_entry *fa = a;
    const font_data_entry *fb = b;
    
    #ifdef _WIN32
    return strcmp_i(fa->filename, fb->filename);
    #else
    return strcmp(fa->filename, fb->filename);
    #endif
}

uint64_t font_data_entry_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const font_data_entry *e = item;
    #ifdef _WIN32
    return case_insensitive_hash(e->filename, seed0, seed1);
    #else
    return hashmap_sip(e->filename, strlen(e->filename), seed0, seed1);
    #endif
}

typedef struct _font_entry {
    const char *familyName;
    int isSystemFont;
    blurg_font_t *regular;
    blurg_font_t *italic;
    blurg_font_t *bold;
    blurg_font_t *boldItalic;
    int listIndex;
} font_entry;

int font_entry_compare(const void *a, const void* b, void *udata)
{
    const font_entry *ga = a;
    const font_entry *gb = b;
    return strcmp_i(ga->familyName, gb->familyName);
}

uint64_t font_entry_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const font_entry* e  = item;
    return case_insensitive_hash(e->familyName, seed0, seed1);
}


#define K_BOLD (BLURG_WEIGHT_BOLD)
#define K_ITALIC ((1U << 31) | BLURG_WEIGHT_REGULAR)
#define K_BOLDITALIC ((1U << 31) | BLURG_WEIGHT_BOLD)
#define K_REGULAR BLURG_WEIGHT_REGULAR

void font_manager_init(blurg_t *blurg)
{
    blurg->fontManager = malloc(sizeof(font_manager_t));
    blurg->fontManager->defaultFont = NULL;
    list_font_lookup_node_init(&blurg->fontManager->nodes, 8);
    blurg->fontManager->fontTable = hashmap_new(sizeof(font_entry), 0, 0, 0, font_entry_hash, font_entry_compare, NULL, NULL);
    blurg->fontManager->fileTable = hashmap_new(sizeof(font_data_entry), 0, 0, 0, font_data_entry_hash, font_data_entry_compare, NULL, NULL);
}

static bool free_files(const void* file, void* udata)
{
    const font_data_entry *de = file;
    free(de->filename);
    if(!de->font->external) {
        free(de->font->data);
    }
    free(de->font);
    return 1;
}

void font_manager_destroy(blurg_t *blurg)
{
    list_font_lookup_node_free(&blurg->fontManager->nodes);
    hashmap_free(blurg->fontManager->fontTable);
    hashmap_scan(blurg->fontManager->fileTable, free_files, NULL);
    hashmap_free(blurg->fontManager->fileTable);
    free(blurg->fontManager);
}

static void font_entry_set_style(font_manager_t *fm, font_entry *entry, int nodeLast, uint32_t key, blurg_font_t *font)
{
    if(key == K_REGULAR) {
        entry->regular = font;
    }
    else if (key == K_ITALIC) {
        entry->italic = font;
    }
    else if (key == K_BOLD) {
        entry->bold = font;
    }
    else if (key == K_BOLDITALIC) {
        entry->boldItalic = font;
    } 
    else {
        int liprev = 0;
        int li = nodeLast ? nodeLast : entry->listIndex;
        while(li) {
            liprev = li;
            if(fm->nodes.data[li - 1].key == key) {
                fm->nodes.data[li - 1].font = font;
                return;
            }
            li = fm->nodes.data[li - 1].nextIndex;
        }
        list_font_lookup_node_add(&fm->nodes, (font_lookup_node){
            .nextIndex = 0, .key = key, .font = font
        });
        if(liprev) {
            fm->nodes.data[liprev - 1].nextIndex = fm->nodes.count;
        } else {
            entry->listIndex = fm->nodes.count;
        }
    }
}

static inline int64_t styleDiff(uint32_t a, uint32_t b)
{
    int64_t sdiff = ((int64_t)b) - ((int64_t)a);
    if(sdiff < 0) return -sdiff;
    return sdiff;
}


static blurg_font_t *query_entry(font_manager_t *fm, const font_entry *entry, int weight, int italic, int *exactMatch)
{
    #define MATCHDIFF(f,k) { int64_t _diff = styleDiff(styleKey, (k)); if(currDifference > _diff) { currDifference = _diff; currFont = (f); }}

    uint32_t styleKey = (italic ? (1U << 31) : 0) | (uint32_t)weight;
    blurg_font_t *currFont = NULL;
    int64_t currDifference = INT64_MAX;
    // check common styles
    if(styleKey == K_REGULAR && entry->regular) {
        *exactMatch = 1;
        return entry->regular;
    } else if(entry->regular) {
        currFont = entry->regular;
        currDifference = styleDiff(K_REGULAR, styleKey);
    }
    if(styleKey == K_ITALIC && entry->italic) {
        *exactMatch = 1;
        return entry->italic;
    } else if(entry->italic) {
        MATCHDIFF(entry->italic, K_ITALIC);
    }
    if(styleKey == K_BOLD && entry->bold) {
        *exactMatch = 1;
        return entry->bold;
    } else if(entry->bold) {
        MATCHDIFF(entry->bold, K_BOLD);
    }
    if(styleKey == K_BOLDITALIC && entry->boldItalic) {
        *exactMatch = 1;
        return entry->boldItalic;
    } else if (entry->boldItalic) {
        MATCHDIFF(entry->boldItalic, K_BOLDITALIC);
    }
    // go through linked list
    int l = entry->listIndex;
    while(l) {
        if(fm->nodes.data[l - 1].key == styleKey) {
            *exactMatch = 1;
            return fm->nodes.data[l - 1].font;
        }
        MATCHDIFF(fm->nodes.data[l - 1].font, fm->nodes.data[l - 1].key);
        l = fm->nodes.data[l - 1].nextIndex;
    }

    *exactMatch = 0;
    return currFont;
    #undef MATCHDIFF
}

allocated_font *load_file_data(blurg_t *blurg, const char *filename)
{
    font_manager_t *fm = blurg->fontManager;
    //discard const* for lookup, we know hashmap_get does not modify the pointer
    const font_data_entry *result = hashmap_get(fm->fileTable, &(font_data_entry){ .filename = (char*)filename });
    if(result) {
        return result->font;
    }
    size_t len;
    char *data = read_all_bytes(filename, &len);
    if(!data) {
        return NULL;
    }
    allocated_font *fd = malloc(sizeof(allocated_font));
    fd->dataLen = len;
    fd->data = data;
    fd->external = 0;
    hashmap_set(fm->fileTable, &(font_data_entry){ .filename = strdup(filename), .font = fd });
    return fd;
}

void add_font(blurg_t *blurg, blurg_font_t *font)
{
    font_manager_t *fm = blurg->fontManager;
    const font_entry *existing = hashmap_get(fm->fontTable, &(font_entry){ .familyName = font->face->family_name });
    font_entry e;
    if(!existing) {
        memset(&e, 0, sizeof(font_entry));
        e.familyName = font->face->family_name;
    } else {
        e = *existing;
    }
    uint32_t key = (font->italic ? (1U << 31) : 0) | (uint32_t)font->weight;
    font_entry_set_style(fm, &e, 0, key, font);
    hashmap_set(fm->fontTable, &e);
}

BLURGAPI blurg_font_t *blurg_font_add_file(blurg_t *blurg, const char *filename)
{
    font_manager_t *fm = blurg->fontManager;
    allocated_font *data = load_file_data(blurg, filename);
    if(!data) {
        return NULL;
    }

    blurg_font_t *font = blurg_font_create_internal(blurg, data);
    add_font(blurg, font);
    return font;
}

BLURGAPI blurg_font_t *blurg_font_add_memory(blurg_t *blurg, char *data, int len, int copy)
{
    font_manager_t *fm = blurg->fontManager;
    allocated_font *fontData = malloc(sizeof(allocated_font));
    fontData->dataLen = len;
    if(copy) {
        fontData->data = malloc(len);
        memcpy(fontData->data, data, len);
        fontData->external = 0;
    } else {
        fontData->external = 1;
    }

    blurg_font_t *font = blurg_font_create_internal(blurg, fontData);

    if(!font) {
        if(copy) {
            free(fontData->data);
        }
        free(fontData);
        return NULL;
    }

    // use invalid filename + map count to create unique identifier
    char identifier[256];
    snprintf(identifier, 256, "COM1:/dev/null/%zu\n", hashmap_count(blurg->fontManager->fileTable));
    hashmap_set(blurg->fontManager->fileTable, &(font_data_entry){ .filename = strdup(identifier), .font = fontData });
    add_font(blurg, font);
    return font;
}

BLURGAPI blurg_font_t *blurg_font_query(blurg_t *blurg, const char *familyName, int weight, int italic)
{
    font_manager_t *fm = blurg->fontManager;
    const font_entry *result = hashmap_get(fm->fontTable, &(font_entry){ .familyName = familyName });
    if(!result) {
        if(fm->defaultFont) {
            result = hashmap_get(fm->fontTable, &(font_entry){ .familyName = fm->defaultFont });
            if(!result) {
                return NULL;
            }
        } else {
            return NULL;
        }
    }
    int exactMatch;
    blurg_font_t *fnt = query_entry(fm, result, weight, italic, &exactMatch);
    if(exactMatch) {
        return fnt;
    }
    return fnt;
}

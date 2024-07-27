#if BT_ENABLE_DIRECTWRITE
#include "blurgtext_internal.h"
#include <dwrite.h>
#include <stdio.h>
// Helpful tools for dealing with the deluge of COM objects required to use DWrite
#define HR(x) if (!SUCCEEDED((x))) { return NULL; }
#define HRE(x) if(!SUCCEEDED((x))) { return E_NOTIMPL; }
class WStr
{
public:
    wchar_t* cstr;

    WStr(const char* input)
    {
        int wsize = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0);
        cstr = (wchar_t*)malloc(wsize * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, input, -1, cstr, wsize);
    }

    ~WStr()
    {
        free(cstr);
    }
};

template<class T> class ScopedCom {
public:
    T* Pointer;
    ScopedCom()
    {
        Pointer = (T*)NULL;
    }
    ~ScopedCom()
    {
        if (Pointer) {
            Pointer->Release();
        }
    }
    T& operator*() const { return *Pointer; }
    T** operator&() { return &Pointer; };
    T* get() { return Pointer; }
};

template<class T> class Array {
public:
    T* Pointer;
    Array(int size) { Pointer = new T[size]; }
    ~Array() { delete[]Pointer; }
    operator T* () { return Pointer; }
};

static blurg_font_t* FromDWriteFace(blurg_t* blurg, IDWriteFontFace* face)
{
    uint32_t count = 0;
    HR(face->GetFiles(&count, NULL));
    Array<IDWriteFontFile*>files(count);
    HR(face->GetFiles(&count, files));
    for (int i = 1; i < count; i++) {
        files[i]->Release();
    }
    ScopedCom<IDWriteFontFile> file;
    file.Pointer = files[0];
    ScopedCom<IDWriteFontFileLoader> loader;
    const void* referenceKey;
    UINT32 referenceKeySize;
    HR(file.get()->GetReferenceKey(&referenceKey, &referenceKeySize));
    HR(file.get()->GetLoader(&loader));
    ScopedCom<IDWriteFontFileStream> stream;
    HR(loader.get()->CreateStreamFromKey(referenceKey, referenceKeySize, &stream));
    UINT64 sz;
    const void* buffer;
    void* fragContext;
    HR(stream.get()->GetFileSize(&sz));
    HR(stream.get()->ReadFileFragment(&buffer, 0, sz, &fragContext));
    blurg_font_t* bfnt = blurg_font_add_memory(blurg, (char*)buffer, (int)sz, 1);
    DWRITE_FONT_SIMULATIONS sims = face->GetSimulations();
    if (sims & DWRITE_FONT_SIMULATIONS_BOLD) {
        bfnt->embolden = 1;
    }
    if (sims & DWRITE_FONT_SIMULATIONS_OBLIQUE) {
        // TODO: Simulate italics
    }
    if (sims) {
        blurg_font_rehash(bfnt);
    }
    stream.get()->ReleaseFileFragment(fragContext);
    return bfnt;
}

uint32_t utf32_to_utf16(uint32_t utf32, uint16_t *utf16)
{
    if (utf32 < 0xD800 || (utf32 > 0xDFFF && utf32 < 0x10000))
    {
        utf16[0] = (uint16_t)utf32;
        utf16[1] = 0;
        return 1;
    }
    utf32 -= 0x010000;
    utf16[0] = (uint16_t)(((0xFFC00 & utf32) >> 10) + 0xD800);
    utf16[1] = (uint16_t)(((0x3FF & utf32) >> 00) + 0xDC00);
    return 2;
}

class FallbackRenderer final : public IDWriteTextRenderer
{
public:
    IDWriteFontCollection* systemFonts;
    long refCount;
    blurg_font_t* resolved;
    blurg_t* blurg;
    uint32_t character;
    FallbackRenderer(IDWriteFontCollection* col, blurg_t *blurg, uint32_t character)
    {
        this->blurg = blurg;
        systemFonts = col;
        refCount = 1;
        resolved = NULL;
        this->character = character;
    }
    ~FallbackRenderer() {}

    IFACEMETHOD(DrawGlyphRun)(
        void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY,
        DWRITE_MEASURING_MODE measuringMode, DWRITE_GLYPH_RUN const* glyphRun,
        DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
        IUnknown* clientDrawingEffect) {
        if (resolved) {
            return S_OK;
        }

        if (!glyphRun->fontFace) {
            return E_INVALIDARG;
        }

        ScopedCom<IDWriteFont> font;
        HRE(systemFonts->GetFontFromFontFace(glyphRun->fontFace, &font));

        BOOL exists;
        HRE(font.get()->HasCharacter(character, &exists));

        if (exists)
        {
            resolved = FromDWriteFace(blurg, glyphRun->fontFace);
        }
        return S_OK;
    };

    IFACEMETHOD(DrawUnderline)(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY,
            DWRITE_UNDERLINE const* underline, IUnknown* clientDrawingEffect) {
        return E_NOTIMPL;
    }

    IFACEMETHOD(DrawStrikethrough)(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY,
            DWRITE_STRIKETHROUGH const* strikethrough, IUnknown* clientDrawingEffect) {
        return E_NOTIMPL;
    }

    IFACEMETHOD(DrawInlineObject)(void* clientDrawingContext, FLOAT originX, FLOAT originY,
            IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft,
            IUnknown* clientDrawingEffect) {
        return E_NOTIMPL;
    }

    IFACEMETHOD(IsPixelSnappingDisabled)(void* clientDrawingContext, BOOL* isDisabled) {
        *isDisabled = FALSE;
        return S_OK;
    }

    IFACEMETHOD(GetCurrentTransform)(void* clientDrawingContext, DWRITE_MATRIX* transform) {
        const DWRITE_MATRIX ident = { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };
        *transform = ident;
        return S_OK;
    }

    IFACEMETHOD(GetPixelsPerDip)(void* clientDrawingContext, FLOAT* pixelsPerDip) {
        *pixelsPerDip = 1.0f;
        return S_OK;
    }

    IFACEMETHOD_(unsigned long, AddRef)() {
        return InterlockedIncrement(&refCount);
    }

    IFACEMETHOD_(unsigned long, Release)() {
        unsigned long newCount = InterlockedDecrement(&refCount);
        if (newCount == 0) {
            delete this;
            return 0;
        }

        return newCount;
    }

    IFACEMETHOD(QueryInterface)(IID const& riid, void** ppvObject) {
        if (riid == __uuidof(IDWriteTextRenderer)) {
            *ppvObject = this;
        }
        else if (riid == __uuidof(IDWritePixelSnapping)) {
            *ppvObject = this;
        }
        else if (riid == __uuidof(IUnknown)) {
            *ppvObject = this;
        }
        else {
            *ppvObject = nullptr;
            return E_FAIL;
        }

        this->AddRef();
        return S_OK;
    }
};

class DWriteFontLookup
{
public:
    IDWriteFactory* factory;
    IDWriteFontCollection* systemFonts;

    DWriteFontLookup(IDWriteFactory *f, IDWriteFontCollection* col)
    {
        this->factory = f;
        this->systemFonts = col;
    }

    ~DWriteFontLookup()
    {
        systemFonts->Release();
        factory->Release();
    }

private:
    blurg_font_t* QueryInternal(blurg_t* blurg, const char* familyName, int weight, int italic)
    {
        WStr familyWide(familyName);

        uint32_t index;
        BOOL exists = 0;
        HRESULT hr = systemFonts->FindFamilyName(familyWide.cstr, &index, &exists);
        if (!SUCCEEDED(hr) || !exists) {
            return NULL;
        }
        ScopedCom<IDWriteFontFamily> family;
        HR(systemFonts->GetFontFamily(index, &family));
        ScopedCom<IDWriteFont> font;
        HR(family.get()->GetFirstMatchingFont((DWRITE_FONT_WEIGHT)weight, DWRITE_FONT_STRETCH_NORMAL, italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, &font));
        ScopedCom<IDWriteFontFace> face;
        HR(font.get()->CreateFontFace(&face));
        return FromDWriteFace(blurg, face.get());
    }

public:
    blurg_font_t* QueryFallback(blurg_t* blurg, const char* familyName, int weight, int italic, uint32_t character)
    {
        WStr familyWide(familyName);

        wchar_t str[2];
        uint32_t len = utf32_to_utf16(character, (uint16_t*)str);

        ScopedCom<IDWriteTextFormat> textFormat;
        HR(factory->CreateTextFormat(
            familyWide.cstr,
            systemFonts,
            (DWRITE_FONT_WEIGHT)weight,
            italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            72.0,
            L"",
            &textFormat
        ));
        ScopedCom<IDWriteTextLayout> layout;
        HR(factory->CreateTextLayout(str, len, textFormat.get(), 1000.0, 1000.0, &layout));
        HR(layout.get()->SetFontCollection(systemFonts, { 0, len }));
        ScopedCom<FallbackRenderer> renderer;
        renderer.Pointer = new FallbackRenderer(systemFonts, blurg, character);
        HR(layout.get()->Draw(NULL, renderer.get(), 2.0, 2.0));
        return renderer.get()->resolved;
    }

    blurg_font_t* Query(blurg_t* blurg, const char* familyName, int weight, int italic)
    {
        blurg_font_t* retval = this->QueryInternal(blurg, familyName, weight, italic);
        if (!retval) {
            return this->QueryInternal(blurg, "Arial", weight, italic);
        }
        return retval;
    }
#undef HR

};

BLURGAPI int blurg_enable_system_fonts(blurg_t* blurg)
{
    if (blurg->sysFontData) {
        return 1;
    }
    IDWriteFactory* dwriteFactory;
    IDWriteFontCollection* systemFonts;
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&dwriteFactory)
    );
    if (!SUCCEEDED(hr)) {
        return 0;
    }
    hr = dwriteFactory->GetSystemFontCollection(&systemFonts);
    if (!SUCCEEDED(hr)) {
        dwriteFactory->Release();
        return 0;
    }
    DWriteFontLookup* lookup = new DWriteFontLookup(dwriteFactory, systemFonts);
    blurg->sysFontData = lookup;
    return 1;
}
blurg_font_t* blurg_sysfonts_query(blurg_t* blurg, const char* familyName, int weight, int italic, uint32_t character)
{
    if (!blurg->sysFontData) {
        return NULL;
    }
    DWriteFontLookup* lookup = (DWriteFontLookup*)blurg->sysFontData;
    if (character) {
        return lookup->QueryFallback(blurg, familyName, weight, italic, character);
    }
    return lookup->Query(blurg, familyName, weight, italic);
}

void blurg_sysfonts_destroy(blurg_t* blurg)
{
    if (blurg->sysFontData) {
        DWriteFontLookup* lookup = (DWriteFontLookup*)blurg->sysFontData;
        delete lookup;
        blurg->sysFontData = NULL;
    }
}
#endif
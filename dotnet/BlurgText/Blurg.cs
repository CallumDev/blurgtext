using System;
using System.Buffers;
using System.Collections.Generic;
using System.Numerics;
using System.Runtime.InteropServices;
using static BlurgText.BlurgNative;

namespace BlurgText
{
    public delegate IntPtr TextureAllocation(int width, int height);
    public delegate void TextureUpdate(IntPtr userData, IntPtr buffer, int x, int y, int width, int height);
    
    public unsafe class Blurg : IDisposable
    {
        public IntPtr Handle { get; private set; }

        private TextureAllocation textureAllocation;
        private TextureUpdate textureUpdate;

        private blurg_texture_allocate nativeAllocate;
        private blurg_texture_update nativeUpdate;
        
        Dictionary<IntPtr, BlurgFont> fontInstances = new Dictionary<IntPtr, BlurgFont>();
        
        public Blurg(TextureAllocation textureAllocation, TextureUpdate textureUpdate)
        {
            this.textureAllocation = textureAllocation;
            this.textureUpdate = textureUpdate;
            nativeAllocate = NativeAllocate;
            nativeUpdate = NativeUpdate;
            Handle = blurg_create(
                Marshal.GetFunctionPointerForDelegate(nativeAllocate),
                Marshal.GetFunctionPointerForDelegate(nativeUpdate)
            );
        }

        public bool EnableSystemFonts() => blurg_enable_system_fonts(Handle) != 0;

        BlurgFont? ToFont(IntPtr ptr)
        {
            if (ptr == IntPtr.Zero)
                return null;
            if (fontInstances.TryGetValue(ptr, out var fnt))
                return fnt;
            fnt = new BlurgFont(ptr);
            fontInstances.Add(ptr, fnt);
            return fnt;
        }

        public BlurgFont? AddFontFile(string filename)
        {
            Span<byte> nbytes = stackalloc byte[512];
            using var native_name = new UTF8ZHelper(nbytes, filename);
            fixed (byte* p = native_name.ToUTF8Z())
                return ToFont(blurg_font_add_file(Handle, (IntPtr)p));
        }

        public BlurgFont? QueryFont(string familyName, FontWeight weight, bool italic)
        {
            Span<byte> nbytes = stackalloc byte[512];
            using var native_name = new UTF8ZHelper(nbytes, familyName);
            fixed (byte* p = native_name.ToUTF8Z())
                return ToFont(blurg_font_query(Handle, (IntPtr)p, weight, italic ? 1 : 0));
        }

        static BlurgResult? GetResult(IntPtr ptr, int count, float w, float h) =>
            ptr == IntPtr.Zero ? null : new BlurgResult(ptr, count, w, h);

        public BlurgResult? BuildString(BlurgFont font, float size, BlurgColor color, string text)
        {
            fixed (char *p = text)
            {
                int rectCount;
                float w, h;
                var rects = blurg_build_string_utf16(Handle, font.Handle, size, color, (IntPtr)p, &rectCount, &w, &h);
                return GetResult(rects, rectCount, w, h);
            }
        }
        
        public Vector2 MeasureString(BlurgFont font, float size, string text)
        {
            float w, h;
            fixed (char* p = text)
            {
                blurg_measure_string_utf16(Handle, font.Handle, size, (IntPtr)p, &w, &h);
            }
            return new Vector2(w, h);
        }

        public BlurgResult? BuildFormattedText(BlurgFormattedText text, float maxWidth = 0) =>
            BuildFormattedText(MemoryMarshal.CreateSpan(ref text, 1), maxWidth);

        public BlurgResult? BuildFormattedText(ReadOnlySpan<BlurgFormattedText> texts, float maxWidth = 0) =>
            FormattedTextCall(false, texts, maxWidth).Result;
        
        public Vector2 MeasureFormattedText(BlurgFormattedText text, float maxWidth = 0) =>
            MeasureFormattedText(MemoryMarshal.CreateSpan(ref text, 1), maxWidth);
        
        public Vector2 MeasureFormattedText(ReadOnlySpan<BlurgFormattedText> texts, float maxWidth = 0) =>
            FormattedTextCall(true, texts, maxWidth).Size;

        struct ResultOrSize
        {
            public BlurgResult? Result;
            public Vector2 Size;
        }
        
        ResultOrSize FormattedTextCall(bool measure, ReadOnlySpan<BlurgFormattedText> text, float maxWidth)
        {
            Span<blurg_formatted_text_t> native = stackalloc blurg_formatted_text_t[text.Length];
            Span<GCHandle> handles = stackalloc GCHandle[text.Length];
            int spanAmount = 0;
            Span<blurg_style_span_t> styles;
            blurg_style_span_t[]? stylesArray = null;
            for (int i = 0; i < text.Length; i++)
            {
                handles[i] = GCHandle.Alloc(text[i].Text, GCHandleType.Pinned);
                spanAmount += text[i].Spans?.Length ?? 0;
                native[i] = new blurg_formatted_text_t()
                {
                    alignment = text[i].Alignment,
                    encoding = blurg_encoding_t.blurg_encoding_utf16,
                    text = handles[i].AddrOfPinnedObject(),
                    defaultFont = text[i].DefaultFont.Handle,
                    defaultSize = text[i].DefaultSize,
                    defaultBackground = text[i].DefaultBackground,
                    defaultColor = text[i].DefaultColor,
                    defaultShadow = text[i].DefaultShadow,
                    defaultUnderline = text[i].DefaultUnderline,
                    spanCount = text[i].Spans?.Length ?? 0,
                    spans = IntPtr.Zero
                };
            }
            
            if (spanAmount > 50) {
                stylesArray = ArrayPool<blurg_style_span_t>.Shared.Rent(spanAmount);
                styles = stylesArray.AsSpan();
            }
            else {
                // We know this stackalloc never escapes the method. Disable compiler warning
#pragma warning disable CS9081 
                styles = stackalloc blurg_style_span_t[spanAmount];
#pragma warning restore CS9081 
            }

            int st = 0;
            IntPtr retVal;
            float w = 0, h = 0;
            int rectCount;
            
            fixed (blurg_style_span_t* sptr = styles)
            {
                for (int i = 0; i < text.Length; i++)
                {
                    if (text[i].Spans != null && text[i].Spans!.Length > 0)
                    {
                        native[i].spans = (IntPtr)(&sptr[st]);
                        for (int j = 0; j < text[i].Spans!.Length; j++)
                        {
                            var span = text[i].Spans![j];
                            styles[st++] = new blurg_style_span_t()
                            {
                                startIndex = span.StartIndex,
                                endIndex = span.EndIndex,
                                background = span.Background,
                                color = span.Color,
                                font = span.Font.Handle,
                                fontSize = span.FontSize,
                                underline = span.Underline,
                                shadow = span.Shadow
                            };
                        }
                    }
                }

                fixed (blurg_formatted_text_t* fmt = native)
                {
                    if (measure)
                    {
                        retVal = IntPtr.Zero;
                        rectCount = 0;
                        blurg_measure_formatted(Handle, (IntPtr)fmt, text.Length, maxWidth, &w, &h);
                    }
                    else
                    {
                        retVal = blurg_build_formatted(Handle, (IntPtr)fmt, text.Length, maxWidth, &rectCount, &w, &h);
                    }
                }
            }
           
            
            if (stylesArray != null) 
            {
                ArrayPool<blurg_style_span_t>.Shared.Return(stylesArray);
            }
            
            for (int i = 0; i < handles.Length; i++)
            {
                handles[i].Free();
            }

            if (measure)
                return new ResultOrSize() { Size = new Vector2(w, h) };
            else
                return new ResultOrSize() { Result = GetResult(retVal, rectCount, w, h) };
        }
        
        private unsafe void NativeAllocate(IntPtr texture, int width, int height)
        {
            ((blurg_texture_t*)texture)->userdata = textureAllocation(width, height);
        }
        
        private unsafe void NativeUpdate(IntPtr texture, IntPtr buffer, int x, int y, int width, int height)
        {
            textureUpdate(((blurg_texture_t*)texture)->userdata, buffer, x, y, width, height);
        }
        
        private void ReleaseUnmanagedResources()
        { 
            blurg_destroy(Handle);
        }

        public void Dispose()
        {
            ReleaseUnmanagedResources();
            GC.SuppressFinalize(this);
        }

        ~Blurg()
        {
            ReleaseUnmanagedResources();
        }
    }
}
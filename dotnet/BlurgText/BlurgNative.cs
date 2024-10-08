using System;
using System.Runtime.InteropServices;

namespace BlurgText
{
    public static class BlurgNative
    {
        [StructLayout(LayoutKind.Sequential)]
        public struct blurg_texture_t
        {
            public IntPtr userdata;
        }

        public enum blurg_encoding_t
        {
            blurg_encoding_utf8 = 0,
            blurg_encoding_utf16 = 1
        }
        
        [StructLayout(LayoutKind.Sequential)]
        public struct blurg_style_span_t 
        {
            public int startIndex;
            public int endIndex;
            public IntPtr font;
            public float fontSize;
            public BlurgColor background;
            public BlurgColor color;
            public BlurgUnderline underline;
            public BlurgShadow shadow;
        } 
        
        [StructLayout(LayoutKind.Sequential)]
        public struct blurg_formatted_text_t
        {
            public IntPtr text;
            public blurg_encoding_t encoding;
            public BlurgAlignment alignment;
            public IntPtr spans;
            public int spanCount;
            public float defaultSize;
            public IntPtr defaultFont;
            public BlurgColor defaultBackground;
            public BlurgColor defaultColor;
            public BlurgUnderline defaultUnderline;
            public BlurgShadow defaultShadow;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct blurg_result_t
        {
            public float width;
            public float height;
            public int rectCount;
            public IntPtr rects;
            public int cursorCount;
            public IntPtr cursors;
        }
        
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void blurg_texture_allocate(IntPtr texture, int width, int height);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void blurg_texture_update(IntPtr texture, IntPtr buffer, int x, int y, int width,
            int height);
        
        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr blurg_create(IntPtr textureAllocate, IntPtr textureUpdate);

        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern int blurg_enable_system_fonts(IntPtr blurg);

        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr blurg_font_add_file(IntPtr blurg, IntPtr filename);

        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr blurg_font_query(IntPtr blurg, IntPtr familyName, int weight, int italic);

        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr blurg_font_get_family(IntPtr font);

        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern float blurg_font_get_line_height(IntPtr font, float size);

        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern int blurg_font_get_weight(IntPtr font);
        
        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern int blurg_font_get_italic(IntPtr font);
        
        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern void blurg_font_set_fallback(IntPtr font, IntPtr fallback);

        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void blurg_build_string_utf16(IntPtr blurg, IntPtr font, float size, uint color,
            IntPtr text, blurg_result_t *result);

        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void blurg_build_formatted(IntPtr formatted, IntPtr texts, int count,
            int measureCursor, float maxWidth, blurg_result_t* result);
        
        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void blurg_measure_string_utf16(IntPtr blurg, IntPtr font, float size, IntPtr text, float *width, float *height);
        
        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void blurg_measure_formatted(IntPtr blurg, IntPtr texts, int count, float maxWidth, float* width, float *height);
        
        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern void blurg_free_result(IntPtr result);

        [DllImport("libblurgtext", CallingConvention = CallingConvention.Cdecl)]
        public static extern void blurg_destroy(IntPtr blurg);
    }
}

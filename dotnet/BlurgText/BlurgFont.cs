using System;
using System.Runtime.InteropServices;

namespace BlurgText
{
    public sealed class BlurgFont
    {
        public IntPtr Handle { get; private set; }
        
        public string FamilyName { get; private set; }
        
        public BlurgFont(IntPtr handle)
        {
            Handle = handle;
            FamilyName = Marshal.PtrToStringUTF8(BlurgNative.blurg_font_get_family(handle))!;
        }
        
        public float LineHeight(float size) => BlurgNative.blurg_font_get_line_height(Handle, size);
        
        public FontWeight Weight => BlurgNative.blurg_font_get_weight(Handle);

        public bool Italic => BlurgNative.blurg_font_get_italic(Handle) != 0;
    }
}
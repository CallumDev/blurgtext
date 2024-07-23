using System.Runtime.InteropServices;

namespace BlurgText
{
    [StructLayout(LayoutKind.Explicit)]
    public struct BlurgColor
    {
        public static readonly BlurgColor White = new BlurgColor(255, 255, 255, 255);
        public static readonly BlurgColor Black = new BlurgColor(0, 0, 0, 255);
        
        [FieldOffset(0)]
        public uint Value;

        [FieldOffset(0)] 
        public byte R;

        [FieldOffset(1)] 
        public byte G;

        [FieldOffset(2)] 
        public byte B;

        [FieldOffset(3)] 
        public byte A;

        public BlurgColor(byte r, byte g, byte b, byte a)
        {
            Value = 0;
            R = r;
            G = g;
            B = b;
            A = a;
        }
        
        
        
        public static implicit operator uint(BlurgColor color)
        {
            return color.Value;
        }
    }
}
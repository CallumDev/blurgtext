using System.Runtime.InteropServices;

namespace BlurgText
{
    [StructLayout(LayoutKind.Sequential)]
    public struct BlurgShadow
    {
        public int Pixels;
        public BlurgColor Color;
    }
}
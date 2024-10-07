using System.Runtime.InteropServices;

namespace BlurgText;

[StructLayout(LayoutKind.Sequential)]
public struct BlurgCursor
{
    public int X;
    public int Y;
    public int Height;
}

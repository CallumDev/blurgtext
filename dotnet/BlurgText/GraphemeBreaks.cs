using System;

namespace BlurgText;

public class GraphemeBreaks
{
    public static unsafe GraphemeBreak[] Get(string str)
    {
        if (string.IsNullOrEmpty(str))
            return Array.Empty<GraphemeBreak>();

        var breaksNative = new byte[str.Length];
        fixed (byte* b = breaksNative)
        {
            fixed (char* s = str)
            {
                BlurgNative.blurg_graphemebreaks_utf16((IntPtr)s, (IntPtr)str.Length, IntPtr.Zero, (IntPtr)b);
            }
        }
        var breaks = new GraphemeBreak[breaksNative.Length];
        for (int i = 0; i < breaksNative.Length; i++)
        {
            breaks[i] = (GraphemeBreak)breaksNative[i];
        }
        return breaks;
    }
}
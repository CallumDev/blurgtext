using System;
using System.Runtime.InteropServices;

namespace BlurgText
{
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct BlurgRect
    {
        private IntPtr texture;
        private int x;
        private int y;
        private int width;
        private int height;
        private float u0;
        private float v0;
        private float u1;
        private float v1;
        private BlurgColor color;

        public IntPtr UserData => ((BlurgNative.blurg_texture_t*)texture)->userdata;
        public int X => x;
        public int Y => y;
        public int Width => width;
        public int Height => height;
        public float U0 => u0;
        public float V0 => v0;
        public float U1 => u1;
        public float V1 => v1;
        public BlurgColor Color => color;
    }
}
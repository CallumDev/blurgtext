using System.Runtime.InteropServices;

namespace BlurgText
{
    [StructLayout(LayoutKind.Sequential)]
    public struct BlurgUnderline
    {
        private BlurgColor color;
        private int useColor;
        private int enabled;

        public BlurgColor Color
        {
            get => color;
            set => color = value;
        }
        public bool UseColor
        {
            get => useColor != 0;
            set => useColor = value ? 1 : 0;
        }
        public bool Enabled
        {
            get => enabled != 0;
            set => enabled = value ? 1 : 0;
        }
    }
}
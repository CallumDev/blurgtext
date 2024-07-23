using System.Runtime.InteropServices;

namespace BlurgText
{
    [StructLayout(LayoutKind.Sequential)]
    public struct FontWeight
    {
        public static FontWeight Thin => new FontWeight(100);
        public static FontWeight ExtraLight => new FontWeight(200);
        public static FontWeight Light => new FontWeight(300);
        public static FontWeight Regular => new FontWeight(400);
        public static FontWeight Medium => new FontWeight(500);
        public static FontWeight SemiBold => new FontWeight(600);
        public static FontWeight Bold => new FontWeight(700);
        public static FontWeight ExtraBold => new FontWeight(800);
        public static FontWeight Black => new FontWeight(900);
        
        public int Value;

        public FontWeight(int value)
        {
            Value = value;
        }

        public static implicit operator int(FontWeight fontWeight) => fontWeight.Value;

        public static implicit operator FontWeight(int value) => new FontWeight(value);

        public override string ToString() => Value switch
        {
            100 =>  "Thin",
            200 =>  "ExtraLight",
            300 =>  "Light",
            400 =>  "Regular",
            500 =>  "Medium",
            600 =>  "SemiBold",
            700 =>  "Bold",
            800 =>  "ExtraBold",
            900 =>  "Black",
            _ => Value.ToString(),
        };
    }
}
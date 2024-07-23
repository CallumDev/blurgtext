namespace BlurgText
{
    public class BlurgFormattedText
    {
        public BlurgAlignment Alignment;
        public string Text;
        public BlurgFont DefaultFont;
        public float DefaultSize;
        public BlurgColor DefaultColor = BlurgColor.White;
        public BlurgUnderline DefaultUnderline;
        public BlurgShadow DefaultShadow;
        
        public BlurgStyleSpan[]? Spans;

        public BlurgFormattedText(string text, BlurgFont defaultFont)
        {
            Text = text;
            DefaultFont = defaultFont;
        }
    }
}
using System.Windows.Media;

namespace RetroFrame
{
    public static class Palette
    {
        public static readonly Brush Face = Make(0xC0, 0xC0, 0xC0);
        public static readonly Brush Light = Make(0xDF, 0xDF, 0xDF);
        public static readonly Brush Hilite = Make(0xFF, 0xFF, 0xFF);
        public static readonly Brush Shadow = Make(0x80, 0x80, 0x80);
        public static readonly Brush Dark = Make(0x00, 0x00, 0x00);
        public static readonly Brush Desktop = Make(0x00, 0x80, 0x80);

        private static Brush Make(byte r, byte g, byte b)
        {
            var brush = new SolidColorBrush(Color.FromRgb(r, g, b));
            brush.Freeze();
            return brush;
        }
    }
}

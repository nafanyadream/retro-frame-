using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace RetroFrame
{
    public static class Glyphs
    {
        public static readonly string[] Close =
        {
            "xx....xx",
            ".xx..xx.",
            "..xxxx..",
            "...xx...",
            "..xxxx..",
            ".xx..xx.",
            "xx....xx"
        };

        public static readonly string[] Minimize =
        {
            "......",
            "......",
            "......",
            "......",
            "......",
            "......",
            "xxxxxx",
            "xxxxxx"
        };

        public static ImageSource Render(string[] rows, Color color)
        {
            var h = rows.Length;
            var w = rows[0].Length;
            var bitmap = new WriteableBitmap(w, h, 96, 96, PixelFormats.Bgra32, null);
            var pixels = new int[w * h];
            var argb = (255 << 24) | (color.R << 16) | (color.G << 8) | color.B;
            for (var y = 0; y < h; y++)
            {
                for (var x = 0; x < w; x++)
                {
                    if (rows[y][x] == 'x')
                    {
                        pixels[y * w + x] = argb;
                    }
                }
            }
            bitmap.WritePixels(new Int32Rect(0, 0, w, h), pixels, w * 4, 0);
            bitmap.Freeze();
            return bitmap;
        }
    }
}

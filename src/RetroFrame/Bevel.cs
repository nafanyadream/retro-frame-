using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace RetroFrame
{
    public enum BevelKind
    {
        Window,
        Raised,
        Pressed,
        Sunken,
        ThinRaised,
        ThinSunken
    }

    public class Bevel : Decorator
    {
        public static readonly DependencyProperty KindProperty = DependencyProperty.Register(
            nameof(Kind), typeof(BevelKind), typeof(Bevel),
            new FrameworkPropertyMetadata(BevelKind.Raised, FrameworkPropertyMetadataOptions.AffectsRender | FrameworkPropertyMetadataOptions.AffectsMeasure));

        public static readonly DependencyProperty FaceProperty = DependencyProperty.Register(
            nameof(Face), typeof(Brush), typeof(Bevel),
            new FrameworkPropertyMetadata(Palette.Face, FrameworkPropertyMetadataOptions.AffectsRender));

        public BevelKind Kind
        {
            get => (BevelKind)GetValue(KindProperty);
            set => SetValue(KindProperty, value);
        }

        public Brush Face
        {
            get => (Brush)GetValue(FaceProperty);
            set => SetValue(FaceProperty, value);
        }

        public Bevel()
        {
            SnapsToDevicePixels = true;
            RenderOptions.SetEdgeMode(this, EdgeMode.Aliased);
        }

        private int Thickness => Kind == BevelKind.ThinRaised || Kind == BevelKind.ThinSunken ? 1 : 2;

        protected override Size MeasureOverride(Size constraint)
        {
            var t = Thickness * 2;
            if (Child == null)
            {
                return new Size(t, t);
            }
            Child.Measure(new Size(System.Math.Max(0, constraint.Width - t), System.Math.Max(0, constraint.Height - t)));
            return new Size(Child.DesiredSize.Width + t, Child.DesiredSize.Height + t);
        }

        protected override Size ArrangeOverride(Size arrangeSize)
        {
            var t = Thickness;
            Child?.Arrange(new Rect(t, t, System.Math.Max(0, arrangeSize.Width - t * 2), System.Math.Max(0, arrangeSize.Height - t * 2)));
            return arrangeSize;
        }

        protected override void OnRender(DrawingContext dc)
        {
            var w = ActualWidth;
            var h = ActualHeight;
            if (w < 2 || h < 2)
            {
                return;
            }
            dc.DrawRectangle(Face, null, new Rect(0, 0, w, h));
            switch (Kind)
            {
                case BevelKind.Window:
                    Edge(dc, 0, 0, w, h, Palette.Light, Palette.Dark);
                    Edge(dc, 1, 1, w - 2, h - 2, Palette.Hilite, Palette.Shadow);
                    break;
                case BevelKind.Raised:
                    Edge(dc, 0, 0, w, h, Palette.Hilite, Palette.Dark);
                    Edge(dc, 1, 1, w - 2, h - 2, Palette.Light, Palette.Shadow);
                    break;
                case BevelKind.Pressed:
                    Edge(dc, 0, 0, w, h, Palette.Dark, Palette.Hilite);
                    Edge(dc, 1, 1, w - 2, h - 2, Palette.Shadow, Palette.Light);
                    break;
                case BevelKind.Sunken:
                    Edge(dc, 0, 0, w, h, Palette.Shadow, Palette.Hilite);
                    Edge(dc, 1, 1, w - 2, h - 2, Palette.Dark, Palette.Light);
                    break;
                case BevelKind.ThinRaised:
                    Edge(dc, 0, 0, w, h, Palette.Hilite, Palette.Shadow);
                    break;
                case BevelKind.ThinSunken:
                    Edge(dc, 0, 0, w, h, Palette.Shadow, Palette.Hilite);
                    break;
            }
        }

        private static void Edge(DrawingContext dc, double x, double y, double w, double h, Brush topLeft, Brush bottomRight)
        {
            dc.DrawRectangle(topLeft, null, new Rect(x, y, w - 1, 1));
            dc.DrawRectangle(topLeft, null, new Rect(x, y, 1, h - 1));
            dc.DrawRectangle(bottomRight, null, new Rect(x + w - 1, y, 1, h));
            dc.DrawRectangle(bottomRight, null, new Rect(x, y + h - 1, w, 1));
        }
    }
}

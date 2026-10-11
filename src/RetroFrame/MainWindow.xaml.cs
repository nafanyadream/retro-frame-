using System;
using System.Reflection;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;

namespace RetroFrame
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            CloseGlyph.Source = Glyphs.Render(Glyphs.Close, Colors.Black);
            MinimizeGlyph.Source = Glyphs.Render(Glyphs.Minimize, Colors.Black);
            var version = Assembly.GetExecutingAssembly().GetName().Version;
            Info.Text = $"Версия программы: {version.ToString(3)}, среда .NET: {Environment.Version}";
        }

        private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            DragMove();
        }

        private void Minimize_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void Close_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}

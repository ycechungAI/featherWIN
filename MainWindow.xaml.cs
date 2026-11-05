using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.Wpf;

namespace FeatherWin;

public partial class MainWindow : Window
{
    private const string HomeUrl = "https://duckduckgo.com/";
    private readonly Shields _shields = new();

    public MainWindow()
    {
        InitializeComponent();
        Loaded += (_, _) => NewTab(HomeUrl);
    }

    private WebView2? Current => (Tabs.SelectedItem as TabItem)?.Content as WebView2;

    private async void NewTab(string url)
    {
        var web = new WebView2();
        var tab = new TabItem { Header = "New tab", Content = web };
        Tabs.Items.Add(tab);
        Tabs.SelectedItem = tab;

        await web.EnsureCoreWebView2Async();
        var core = web.CoreWebView2;
        _shields.Attach(core);
        core.NewWindowRequested += (_, e) =>
        {
            e.Handled = true;
            NewTab(e.Uri);
        };
        core.DocumentTitleChanged += (_, _) => tab.Header = Shorten(core.DocumentTitle);
        core.NavigationStarting += (_, e) =>
        {
            if (ReferenceEquals(Tabs.SelectedItem, tab))
            {
                AddressBox.Text = e.Uri;
            }
        };
        web.Source = new Uri(url);
    }

    private void Tabs_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        var source = Current?.Source;
        if (source is not null)
        {
            AddressBox.Text = source.ToString();
        }
    }

    private void CloseTab(TabItem tab)
    {
        if (tab.Content is WebView2 web)
        {
            web.Dispose();
        }
        Tabs.Items.Remove(tab);
        if (Tabs.Items.Count == 0)
        {
            NewTab(HomeUrl);
        }
    }

    private void Back_Click(object sender, RoutedEventArgs e)
    {
        if (Current?.CoreWebView2?.CanGoBack == true)
        {
            Current.CoreWebView2.GoBack();
        }
    }

    private void Forward_Click(object sender, RoutedEventArgs e)
    {
        if (Current?.CoreWebView2?.CanGoForward == true)
        {
            Current.CoreWebView2.GoForward();
        }
    }

    private void Reload_Click(object sender, RoutedEventArgs e) => Current?.CoreWebView2?.Reload();

    private void NewTab_Click(object sender, RoutedEventArgs e) => NewTab(HomeUrl);

    private void Shields_Changed(object sender, RoutedEventArgs e) => _shields.Enabled = ShieldsBox.IsChecked == true;

    private void AddressBox_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter && Current is { } web)
        {
            web.Source = new Uri(ToUrl(AddressBox.Text));
            e.Handled = true;
        }
    }

    private void Window_KeyDown(object sender, KeyEventArgs e)
    {
        var ctrl = Keyboard.Modifiers.HasFlag(ModifierKeys.Control);
        if (e.Key == Key.F5 || (ctrl && e.Key == Key.R))
        {
            Reload_Click(sender, e);
        }
        else if (ctrl && e.Key == Key.T)
        {
            NewTab(HomeUrl);
        }
        else if (ctrl && e.Key == Key.W && Tabs.SelectedItem is TabItem tab)
        {
            CloseTab(tab);
        }
        else if (ctrl && e.Key == Key.L)
        {
            AddressBox.Focus();
            AddressBox.SelectAll();
        }
    }

    // What the address bar means: a web address, a bare domain, or a search.
    public static string ToUrl(string input)
    {
        var text = input.Trim();
        if (Uri.TryCreate(text, UriKind.Absolute, out var absolute)
            && (absolute.Scheme == Uri.UriSchemeHttp || absolute.Scheme == Uri.UriSchemeHttps))
        {
            return absolute.ToString();
        }
        if (!text.Contains(' ') && text.Contains('.'))
        {
            return "https://" + text;
        }
        return "https://duckduckgo.com/?q=" + Uri.EscapeDataString(text);
    }

    private static string Shorten(string title) => title.Length <= 24 ? title : title[..23] + "…";
}

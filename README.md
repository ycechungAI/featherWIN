# featherWIN

Like a Feather, the tiny, simple Windows web browser. The Windows sibling of
[Feather](https://github.com/ycechungAI/feather) (macOS, Swift + WebKit).

Windows has no WebKit, so featherWIN uses **Microsoft WebView2**, the Chromium engine that Edge
and most Windows apps use. It's built with **C# and WPF** on .NET 8.

**Status: early scaffold, not yet built or run on Windows.** The code is written but untested.

## What works in this scaffold

- Tabs (Ctrl+T new, Ctrl+W close)
- Address bar: enter a web address, a bare domain or a search
- Back, forward, reload (F5 or Ctrl+R), Ctrl+L focuses the address bar
- Ad and tracker blocking (Shields) with a checkbox to turn it off, from a starter blocklist in
  `Shields.cs`
- Popups open as new tabs

## Planned, after the core

- Full blocklist (port Feather's rules) and per-site Shields
- Bookmarks and bookmark bar
- History, downloads, and session restore
- Find on page, private windows

Feather's README lists the full feature set; featherWIN follows it as the milestones land.

## Build and run (on Windows)

You need:

- Windows 10 (1809 or later) or Windows 11
- [.NET 8 SDK](https://dotnet.microsoft.com/download/dotnet/8.0)
- The WebView2 Runtime. Windows 11 and recent Windows 10 have it already; otherwise install the
  Evergreen runtime from Microsoft.

```powershell
git clone https://github.com/ycechungAI/featherWIN
cd featherWIN
dotnet build
dotnet run
```

## Layout

| File | What it is |
|---|---|
| `FeatherWin.csproj` | project file, .NET 8, WPF, WebView2 package |
| `App.xaml`, `App.xaml.cs` | application entry point |
| `MainWindow.xaml`, `MainWindow.xaml.cs` | window, tabs, address bar, keyboard shortcuts |
| `Shields.cs` | ad and tracker blocking through WebView2 request filtering |
| `app.manifest` | per-monitor DPI awareness |

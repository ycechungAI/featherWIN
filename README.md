# featherWIN

A tiny, simple Windows web browser — the Windows sibling of
[Feather](https://github.com/ycechungAI/feather) (macOS).

featherWIN is **Windows-only** and uses **Microsoft Edge WebView2** (Chromium engine) from
**C++ / Win32** with the WebView2 SDK (see [docs/adr/0001-webview2-engine.md](docs/adr/0001-webview2-engine.md)).
It uses the WebView2 runtime already present on supported Windows systems (no bundled browser engine).

## Status

Active development. The app builds and runs on Windows; features and compatibility are still evolving.
See [SPEC.md](SPEC.md), [ROADMAP.md](ROADMAP.md), and [docs/](docs/).

## WebView2 and extensions (plain English)

**WebView2 gives featherWIN the Chromium *engine*** — so web pages generally behave like they do in
Chrome/Edge.

**It does *not* give featherWIN the full Chrome browser extension platform.** Chrome Web Store
install, extension toolbar buttons/action UI, and full Chrome-like extension behavior are browser
features that WebView2 does not fully expose to host apps.

What works (best-effort, varies by extension):
- Load **unpacked** extension folders (with `manifest.json`) from `%APPDATA%\featherWIN\extensions\`
- **Extensions > Add Unpacked Extension...** and **Install Extension from ZIP/CRX...** (local files only)
- Open extension popup/options pages from the **Extensions** menu when available

What does **not** work like Chrome/Firefox:
- Chrome Web Store one-click install / store `.crx` install flow
- Native extension toolbar action buttons
- Firefox `.xpi` / AMO extensions (different engine; WebView2 is Chromium-based)
- Some extensions (especially those relying on Chrome-specific UI/APIs) may fail or behave differently

For password managers like Bitwarden, consider the **official Bitwarden Windows desktop app** if the
browser extension is unreliable in WebView2. featherWIN includes a built-in **Picture in Picture**
button (`FR-VID1`) as a simpler alternative to PiP extensions.

## Privacy

featherWIN aims for a simple, private browser: no telemetry, no accounts, no background processes,
and no app network calls of its own (except browsing and search suggestions when you enable them).
See `C-PRIV` in [SPEC.md](SPEC.md). WebView2 runtime behavior should be verified before release.

## Build (Windows)

Requirements:
- Windows 10/11
- Visual Studio Build Tools / MSVC (C++), CMake, and Windows SDK
- WebView2 SDK is fetched/configured by the project’s CMake setup (see [CMakeLists.txt](CMakeLists.txt))

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\featherwin.exe
```

## Layout (key files)

| Path | Purpose |
|---|---|
| `src/main.cpp` | Win32/WebView2 app (tabs, chrome/UI, bookmarks, downloads/history/settings, extensions, PiP) |
| `src/bookmarks.*` | Bookmark tree and Netscape import/export helpers |
| `src/url.*` | URL/search input handling (`FR-NAV1/2`) |
| `src/blocklist.*` | Shields host blocking helpers (`FR-SHD*`) |
| `tests/logic_test.cpp` | Pure logic tests |
| `spikes/` | WebView2/feature spikes (e.g., extension popup/options) |
| `SPEC.md`, `ROADMAP.md`, `docs/adr/` | Requirements, plan, and decisions |

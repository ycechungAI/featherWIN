# Extension popup spike (FR-EXT1, FR-EXT5)

Question: can WebView2 load a Chromium extension and show its toolbar popup page? This decides how much
extension UI featherWIN has to build itself.

The program loads one unpacked extension with `AreBrowserExtensionsEnabled`, reads its id, then opens
`chrome-extension://<id>/popup.html` in a WebView2 window and prints the result.

## Run it on Windows 11 (the Parallels VM)

Prerequisites: Visual Studio 2022 with "Desktop development with C++", and CMake (included with Visual
Studio, or `winget install Kitware.CMake`). The WebView2 Runtime is already on Windows 11.

```powershell
git clone https://github.com/ycechungAI/featherWIN
cd featherWIN
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target ext_popup_spike
.\build\Release\ext_popup_spike.exe .\spikes\b-extension-popup\test-extension
```

## What to report

Copy the console lines and say what the window shows. Expected lines when it works:

```
loading extension from ...\test-extension
extension loaded: "featherWIN popup test" (id ...)
navigating to chrome-extension://.../popup.html
navigation succeeded (web error status 0), page title "featherWIN popup works"
```

If the window shows "Popup works", the extension popup opens in WebView2. If a line says `FAILED`
or a call failed with an HRESULT, paste that line. Any other extension folder can be tried the same way
(an unpacked folder with a `manifest.json`), and the second argument picks another page.

## Notes

- This is a feasibility spike, not product code. The window only shows the popup page.
- Built and run on Windows 11 (x64 build). Result: the popup page opened with the title "featherWIN
  popup works". See `.agent-memory/STATE.md`.

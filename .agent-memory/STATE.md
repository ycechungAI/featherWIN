# Current state

_Last updated: 2026-11-04 by OpenCode (Haiku 5.5). Engine: WebView2 from C++ (see docs/adr/0001-webview2-engine.md)._

## Done

- Project files: AGENTS.md, SPEC.md (updated for WebView2, extensions allowed with limits), ROADMAP.md (spikes on WebView2), README.md, CHANGELOG.md.
- ADR 0001 (WebView2 engine, supersedes Servo) in docs/adr/.
- Pure logic with tests that run on any OS: `src/blocklist.*` (HostOf, IsBlockedHost), `src/url.*` (ToUrl). Run `sh tests/run_logic_tests.sh` (passes on macOS).
- Extension popup spike (`spikes/b-extension-popup/`): C++ console program that loads an unpacked extension into WebView2 and opens its popup page, plus a test extension. Checked against WebView2 1.0.2739.15 headers; NOT compiled yet. Needs a Windows run.
- CMakeLists.txt at the root: downloads the WebView2 SDK from NuGet and builds `ext_popup_spike`.

## Next steps

1. Run the popup spike on Windows 11 (Parallels VM). Steps in `spikes/b-extension-popup/README.md`. Record the output in this file.
2. Based on the result: decide the extension UI approach (toolbar, popup, options). Options are in the chat history: test more extensions, or build a host for the UI.
3. Then build the browser window (Spike A from ROADMAP): tabs, address bar, Shields (`src/shields.*`, not written yet).
4. PiP: check whether the page's own PiP works in WebView2 (Chromium). Extension fallback if not.

## Gotchas

- Shields and the browser window (`src/main.cpp`) are not written yet; the spike is the only Windows code so far.
- The Servo spike in `spikes/a-servo/` is superseded and kept as history. Don't delete it.
- Pre-existing project files were committed in the first scaffold commit (`d651c0a`) by mistake; they're the project's own files, not a problem, but the history is noisy.
- WebView2 extension install is from unpacked folders only; Chrome Web Store install is not supported.
- WebView2 runtime telemetry and update behavior must be checked against SPEC `C-PRIV` before a release.

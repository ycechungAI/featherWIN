# ADR 0001: Engine is Microsoft Edge WebView2 (supersedes the Servo choice)

Date: 2026-11-04. Status: accepted.

## Context

The first decision in `.agent-memory/DECISIONS.md` picked Servo (Rust) and ruled out Chromium-based engines, including WebView2. The project is Windows-only, and Servo is early: site compatibility, video playback and extension-free rendering all need spikes before the browser can be used.

## Decision

featherWIN uses the WebView2 control (the Chromium engine that ships with Windows), written in C++ with Win32 and the WebView2 SDK. The spec's requirement `C-ENG` is updated to match.

## Consequences

- Better site compatibility, video playback and tab suspend (`TrySuspend`) than Servo today; a smaller app than bundling a browser engine.
- The project now uses a Chromium engine. The privacy stance (`C-PRIV`) still applies to the app's own code. Runtime diagnostics and update behavior of the WebView2 runtime are not yet verified and must be checked before a release.
- Shields uses WebView2's request filter (`WebResourceRequested`), the same approach Servo would have used.
- Browser extensions are possible but limited. WebView2 can load Chromium extensions with `CoreWebView2Profile.AddBrowserExtensionAsync`, after `CoreWebView2EnvironmentOptions.AreBrowserExtensionsEnabled` is set. Installing from the Chrome Web Store is not supported by WebView2, and browser UI for extensions (toolbar, popups, options pages) is limited. Which extensions work is unverified and must be tested on Windows before `FR-EXT*` is committed (Phase 3).
- The Servo spike in `spikes/a-servo/` is kept as history, not deleted, and is no longer the plan.

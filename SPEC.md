# featherWIN specification

Status: draft v0.2. Based on the Feather macOS README (v0.2.0) and the Feather source (cloned to `~/dev/feather-src`, about 9,000 lines of Swift). Section 6 lists what the source shows. Items still marked **[verify]** need a spike.

## 1. Goal

A tiny Windows browser with the same feature set and feel as Feather for macOS, built on the Microsoft Edge WebView2 control (Chromium engine). It adds no rendering engine of its own and sends no data anywhere except the sites the user visits.

## 2. Constraints

| ID | Constraint |
| --- | --- |
| `C-ENG` | The web engine is Servo (Rust, `servo` crate embedding API). No Chromium-based engine (WebView2, CEF, Electron) and no Gecko. See DECISIONS.md. |
| `C-ADDON` | No browser extension support (Servo has none). `FR-EXT*` is out of scope for now. |
| `C-SIZE` | Target installer size under 5 MB, not counting the WebView2 runtime (preinstalled on Windows 10 21H2+ and Windows 11). |
| `C-PRIV` | No telemetry, no accounts, no background processes, no login items, no network calls of its own. |
| `C-OS` | Windows 10 (21H2) and Windows 11, x64 and ARM64. |

## 3. Feature requirements

Each requirement lists the Feather macOS behavior it mirrors.

### Browsing

- `FR-NAV1` Address bar with suggestions from history and bookmarks; ↑/↓ and Enter, or click. Search-engine suggestions off by default.
- `FR-NAV2` Search engines: Google, DuckDuckGo, Bing, Brave Search, Startpage, or a custom URL. Home page setting.
- `FR-NAV3` Find on page (Ctrl+F): Enter / Ctrl+G next, Shift+Enter / Ctrl+Shift+G previous, Esc closes.
- `FR-NAV4` Reopen closed tab (Ctrl+Shift+T), up to 20 tabs, with back/forward history.
- `FR-NAV5` Restore last session after quit or crash. Only the visible tab per window loads immediately; others load when clicked.

### Tabs

- `FR-TAB1` Custom tab strip under the toolbar. Fixed width, shrinking when full; overflow list (») at the right.
- `FR-TAB2` New tab (Ctrl+T or double-click empty strip), close (× / middle click / Ctrl+W), switch (Ctrl+1–8, Ctrl+9 last, Ctrl+Tab / Ctrl+Shift+Tab).
- `FR-TAB3` Drag to reorder; drag off the strip to a new window; drag onto another window to move it there.
- `FR-TAB4` Right-click menu: New Tab, Reload, Mute, Pin, Add to Group, Remove from Group, Close, Close Others, Move to New Window.
- `FR-TAB5` Pinned tabs: narrow, site-initial badge, kept by Close Others, saved with session.
- `FR-TAB6` Sleep: background tabs idle for 5 minutes (configurable) free their page using `TrySuspend`/`Suspend`. All hidden tabs sleep when the system is low on memory (`MemoryUsageLevel` low). Going Back reloads the page.
- `FR-TAB7` Audio indicator (speaker icon in title); Mute Tab (Ctrl+Shift+M) silences the tab.

### Tab groups

- `FR-GRP1` Create group from window's tabs or current tab; name and colour. Saved in `tab-groups.json`.
- `FR-GRP2` Colored chip at the left of the bookmark bar showing first 3 letters; click to bring forward or reopen; right-click for Rename, Change Colour, Open in New Window, Close Group, Delete Group.
- `FR-GRP3` In the tab strip, group tabs sit together under a coloured label; click the label to collapse/expand.
- `FR-GRP4` Groups follow their tabs (navigate, open, close update the saved group). Not available in private windows.

### Bookmarks

- `FR-BM1` Star button and Ctrl+D save to the always-visible bookmark bar.
- `FR-BM2` Drag to reorder with an insertion marker; drop on a folder to file it; folders nest.
- `FR-BM3` Folder popups; » shows overflow.
- `FR-BM4` Right-click: open in new tab, rename, delete, New Folder with Bookmark; Ctrl+click opens in background tab.
- `FR-BM5` Import / export the standard bookmarks `.html` file, preserving folders.

### Sessions

- `FR-SES1` Save the frontmost window's tabs as a named session; restore into the frontmost window or a new one; rename and delete.

### History

- `FR-HIS1` Show All History (Ctrl+H): search, open, delete; right-click "Delete All from This Site".
- `FR-HIS2` Clear History: last hour, today, today and yesterday, or all; optionally cookies, cache and site data. Keeps sign-ins to Google, Gmail and YouTube.
- `FR-HIS3` Private windows never record history. History is local only, kept 7 days.

### Shields (ad and tracker blocking)

- `FR-SHD1` Blocking rules compiled and applied by the engine. **[verify]** WebView2 has no WebKit-style content rules; decide between an in-app request filter (`WebResourceRequested`) and a bundled rule list. See DECISIONS.md.
- `FR-SHD2` Per-site toggle (site and subdomains), shield icon shows a slash when off for a site. Manage site exceptions in Settings.
- `FR-SHD3` Block everywhere toggle.
- `FR-SHD4` Optional larger blocklist from StevenBlack hosts, updated by a script; Reload Blocklists command.
- `FR-SHD5` YouTube video ads muted and play to end so they don't interrupt the video. **[verify]** feasibility on WebView2.
- `FR-SHD6` Tracking parameters removed from URLs.

### Video

- `FR-VID1` Picture in Picture button floats the page's video on any site; reopen if the floating window is closed by a next video or ad.
- `FR-VID2` Download video (Ctrl+Alt+S): saves the page's video while playing into Downloads. Plain files download directly; HLS streams saved as `.mp4` or `.ts`; live streams record until cancelled. DRM and DASH are not supported (same as macOS).
- `FR-VID3` Subtitles: list the video's own tracks, toggle on/off; Load Subtitle File… for `.srt` / `.vtt`.

### Downloads

- `FR-DL1` Downloads panel (Ctrl+J): progress, cancel, Show in Explorer, open, last 50 downloads. Clearing the list keeps the files.
- `FR-DL2` Downloads go to the user's Downloads folder. Mark-of-the-Web is set on downloads (Windows equivalent of Gatekeeper quarantine). **[verify]** the engine sets it by default.

### Extensions

- `FR-EXT1` Install from addons.mozilla.org via "Add to featherWIN" button; clicking any `.xpi` link starts an install. Limited to what WebView2 supports (`C-ADDON`).
- `FR-EXT2` Installed add-ons pinned to the toolbar; right-click to unpin, open settings, remove.
- `FR-EXT3` Extensions menu: Install from File (`.xpi`, `.zip`, or unpacked folder), Manage Extensions (on/off, settings, remove).
- `FR-EXT4` Before install, list the requested permissions. Extensions never run in private windows.
- `FR-EXT5` Known limits are shown in the UI and docs; unsupported add-ons fail with a clear message.

### Settings

- `FR-SET1` Settings (Ctrl+,): search engine, home page, background sleep delay, cache limit (250 MB–5 GB, default 500 MB), Shields, HTTPS-only mode, search suggestions, restore on launch, site exceptions, default-browser button, developer tools toggle.
- `FR-SET2` Changes apply immediately. Settings stored in the user's app data folder (`%APPDATA%\featherWIN`).
- `FR-SET3` Debug logging (`FEATHERWIN_DEBUG=1`) logs requests, saved downloads, failed loads, page crashes.

### Safety

- `FR-SAF1` HTTPS-only mode (upgrade http to https; toggle in Settings).
- `FR-SAF2` Fraudulent-site warnings via the engine's built-in protection (**[verify]**).
- `FR-SAF3` `javascript:` links and opening other apps are handled as in Feather's navigation rules. **[verify]**
- `FR-SAF4` Private windows (Ctrl+Shift+N).
- `FR-SAF5` Developer tools (Inspect Element and DevTools) off by default.

## 4. Non-functional

- `NF-1` Launch to first page in under 1 s on a typical Windows 11 laptop (compare Feather's 0.70 s on M4).
- `NF-2` Memory with one tab under 200 MB excluding WebView2 runtime baseline (compare Feather's 145 MB on macOS). **[verify]** WebView2 processes are shared; measure.
- `NF-3` Works offline for its own UI; no network calls other than browsing and optional search suggestions.
- `NF-4` Installer: a single signed `.exe` or `.msix`; uninstall leaves no background tasks.

## 5. Out of scope (for now)

- Gecko (Firefox) engine.
- Sync, accounts, password manager.
- macOS or Linux builds.
- Notarization equivalent: code signing for Windows is a release step (planned, see ROADMAP).

## 6. What the Feather source shows (checked 2026-11-04)

Source files (Sources/Feather): the engine-specific parts are small.

| Area | Feather (macOS) implementation | featherWIN implication |
| --- | --- | --- |
| Shields (`Shields.swift`, `Blocklist.swift`) | `WKContentRuleList` compiled into `WKContentRuleListStore` | No WebView2 equivalent. Port as a request filter on `WebResourceRequested` with the same blocklist (`FR-SHD1` becomes that). Blocking is per request, so it's slower than compiled rules; measure in Spike B. |
| Extensions (`Extensions.swift`, `ExtensionMenu.swift`) | `WKWebExtension` / `WKWebExtensionController` (WebKit's WebExtension support, Safari-compatible) | WebView2 has no WebExtension host API. Chrome extension loading in WebView2 is limited or unsupported. `FR-EXT*` are at risk; Spike D decides. Be explicit with the user before committing to this feature. |
| Picture in Picture (`MediaScripts.swift`) | Page JavaScript that overrides `disablePictureInPicture` and uses `webkitPresentationMode` | The JS can be injected with WebView2's script-on-document-created API. The presentation check must change to Chromium's `requestPictureInPicture` / `document.pictureInPictureElement`. `FR-VID1` is feasible with code changes. |
| Tab sleep (`TabSuspender.swift`) | Sweep every few minutes; memory pressure sleeps all hidden tabs | Map to WebView2 `TrySuspend` / `Suspend` and the Windows memory notification API. `FR-TAB6` feasible. |
| Navigation rules (`NavigationPolicy.swift`) | `javascript:` navigation is always cancelled; download, add-on install and external-app decisions | Portable logic. Keep the same decision function and its tests (`NavigationPolicyTests.swift`). `FR-SAF3` is verified. |
| Fraud warnings (`BrowserWindowController.swift`) | `isFraudulentWebsiteWarningEnabled` | WebView2 has Microsoft SmartScreen; confirm the setting exists in the WebView2 API (Spike A). `FR-SAF2` is likely feasible. |
| HTTPS upgrade (`URLTools.swift`) | Pure URL function `httpsUpgrade` | Portable as is. `FR-SAF1` verified. |
| Downloads (`Downloads.swift`) | Relies on `LSFileQuarantineEnabled` in Info.plist | Windows Mark-of-the-Web is set by the OS for downloads from the internet zone. Still check in Spike E. |
| Tests | 15 test files, unit-level | The pure-logic tests (URL tools, navigation policy, tab model, bookmarks, sessions, history, tab groups) port directly. Engine-bound tests need rewriting. |

Verified test surface for porting: `URLToolsTests`, `NavigationPolicyTests`, `TabStripModelTests`, `TabGroupsTests`, `BookmarksTests`, `SessionsTests`, `SessionRestoreTests`, `DownloadsHistoryTests`, `SuggestionsTests`, `SearchEngineTests`, `VideoToolsTests`, `SiteDataTests`, `ShieldsAllowlistTests`, `ShieldsRulesTests`, `PageScriptTests`.

Main risk: `FR-EXT*`. Feather's extension support is built on WebKit's WebExtension API, and WebView2 doesn't expose one. Decide before Phase 3 whether to drop it, limit it, or switch engines.

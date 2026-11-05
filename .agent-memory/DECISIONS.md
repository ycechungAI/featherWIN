# Decisions

Append-only. Newest entry at the top.

## 2026-11-05 Sessions (FR-SES1 core, single-window) and minimal Tab Groups (FR-GRP1-4)

**Decision:** Completed practical `FR-SES1` in a single-window app: save named session, restore **add to this window** or **replace this window** (frontmost-window behavior; "new window" restore not supported), rename, delete to Recycle Bin (AGENTS.md), open sessions folder. Implemented a minimal `FR-GRP1-4` slice in `src/main.cpp`: `TabGroup` model + `tab-groups.json` persistence (simple hand-written/parsed JSON), create from current/window tabs, manage groups via menu (rename/color/collapse-state toggle/ungroup/close group tabs), `groupId` added to `Tab` and to session save/load (backward-compatible old 3-column format supported), and a minimal grouped-tab color marker in the tab strip.
**Why:** delivers the requested "session then tab groups" order with compile-safe, incremental progress while keeping size/deps low.
**Open:** `FR-SES1` still lacks "restore in a new window" (needs multi-window architecture) and richer management UI. `FR-GRP2` bookmark-bar colored chip/click/right-click, `FR-GRP3` strip group labels + real collapse layout/hit-testing, full `FR-GRP4` follow-tabs updates, and tab context-menu Add/Remove-from-Group remain. Collapse toggles state but doesn't yet hide tabs in the strip. Rebuild/relink blocked by running `featherwin.exe` (PID 9228); runtime verification needed.

## 2026-11-05 Download handler: fix extension/file download errors; add local ZIP/CRX import

**Decision:** `ICoreWebView2DownloadStarting` now sanitizes filenames, uniquifies the target in the user's Downloads folder when a file already exists (fixes collision-related "download error"), and posts an info hint for `.crx` saves (`FR-DL2` robustness). Added **Extensions > Install Extension from ZIP/CRX...**: user picks a local archive (no app network calls, `C-PRIV`), the app extracts via PowerShell `Expand-Archive` (Windows built-in), for `.crx` it strips the CRX header to the embedded ZIP payload before extraction, finds the folder containing `manifest.json`, and loads it with `AddBrowserExtension` when the profile API is available.
**Why:** Users hit "download error" installing extensions like Bitwarden/PiP from Chrome Web Store flows; WebView2 cannot install from the Chrome Web Store or `.crx`, and filename collisions in the forced Downloads path could fail. Local ZIP/CRX import gives a practical path without adding dependencies or app-initiated network calls.
**Open:** Chrome Web Store in-browser install remains unsupported by WebView2; Firefox extensions are not compatible (Gecko). Some `.crx` packages (store-signed/updated formats) may not load even after header stripping; prefer developer-provided unpacked/ZIP builds. `featherwin.exe` was running during this change (LNK1104 relink) — rebuild after closing the app and verify runtime behavior.

## 2026-11-05 Bookmarks: fix nested deserialization + cycle moves; 2-row bar

**Decision:** Fixed `DeserializeBookmarks` in [src/bookmarks.cpp](src/bookmarks.cpp): the `E` (end-folder) handler double-popped the stack after depth normalization, which could move nested folder contents to the wrong parent on load (matching "disappear/break down/reappear"). `MoveNode` now rejects moves where the target parent is the node itself or a descendant. Bookmark bar layout changed to 2 rows with wrapping/overflow in [src/main.cpp](src/main.cpp).
**Why:** restore bookmark tree integrity and reduce overflow/layout errors; no new dependencies.
**Open:** existing `bookmarks.txt` may already contain structure corrupted by the old parser; user may need backup/Netscape import to recover. Runtime verification needed after rebuild.

## 2026-11-05 History/Downloads/PiP/session UI pass in `src/main.cpp` (source; relink pending)

**Decision:** History is shown as an internal tab (`NavigateToString` HTML) rather than a popup; links post `nav|url` via `chrome.webview.postMessage` and the host navigates the sender (`FR-HIS1` minimal). Downloads use `ICoreWebView2_4::add_DownloadStarting` and set `ResultFilePath` to `%USERPROFILE%/Downloads/<basename>` (`FR-DL2` minimal); a minimal in-memory/downloads view exists and needs Feather-like panel polish (`FR-DL1`). Self-PiP (`FR-VID1`) uses page JS `requestPictureInPicture()` from a toolbar button and File menu; a media dropdown beside PiP opens a **vertical 3-item menu**: 1) Downloads, 2) Subtitles (stub), 3) extension slot configured in `settings.txt` (`media_ext_slot3`) — real extension action invocation is limited because WebView2 does not expose toolbar extension actions. Session/pinned tabs persist in `session.txt` (saved on `WM_CLOSE`, restored on startup). Extensions/Shields/Settings are surfaced via menu bar/gear menus.
**Why:** matches user requests (History tab list, grouped media dropdown beside PiP, pin+session like Feather, Extensions/Shields/settings available) while keeping changes small and avoiding new frameworks/services (`C-PRIV`).
**Open:** rebuild/relink blocked while `featherwin.exe` runs; runtime verification needed for session restore async order, download filename edge cases/MOTW, PiP across sites/ads, FR-TAB6 suspend, and extension-slot-3 invocation.

## 2026-11-05 Shields: never block on YouTube; lightweight FR-SHD2/3/6 in `src/main.cpp`

**Decision:** Shields do not block requests on YouTube (`youtube.com`, `www/m/music.youtube.com`, `youtu.be` and subdomains). The `WebResourceRequested` handler skips blocking when the page host is YouTube, per explicit user request ("YouTube detects ad blocking very well"). Per-site exceptions (`FR-SHD2`) are stored as host lines in `%APPDATA%\featherWIN\shields-exceptions.txt`; the shield button toggles the current site, shows a slash when Shields are off for the site or globally (`FR-SHD2/3`), and the gear menu provides global Shields toggle, per-site toggle, and "Manage Shield Exceptions" (opens the file). Common tracking parameters are stripped on navigation start (`FR-SHD6`, conservative list including `utm_*`, `fbclid`, `gclid`, `msclkid`, etc.).
**Why:** avoids YouTube ad-block detection while keeping blocking simple/low-overhead elsewhere; text-file exceptions and in-app navigation cleanup avoid extra services/frameworks (fits ease-of-use/small-install/low-memory priorities).
**Open:** `FR-SHD4` larger blocklist reload (optional local/script-based, no app network calls per `C-PRIV`), `FR-SHD5` is not pursued on YouTube given the exemption; per-site UI could later show exception count/list in a dialog. Tracking-param stripping should be tested for site breakage/redirect loops.

## 2026-11-05 Browser UI restored in `src/main.cpp` (bookmarks + FR-SHD1 baseline)

**Decision:** Rebuilt `src/main.cpp` as a single Win32/WebView2 implementation after a broken intermediate rewrite. It restores tabs/address bar, a Chromium-like custom chrome (small menu labels, gear near extensions area, theme `RGB(246,247,249)`), rounded owner-drawn MDL2 toolbar buttons, an always-visible bookmark bar with tree bookmarks/folder popups and Netscape import/export UI, and a basic Shields toggle using `WebResourceRequested` + `HostOf`/`IsBlockedHost` (`FR-SHD1`).
**Why:** the file had become uncompilable and untracked; restoring a working, testable baseline unblocks the requested bookmark completion and the upcoming Shields work without adding frameworks (matches ease-of-use/small-install/low-memory priorities).
**Open:** bookmark popup drag/drop across folder levels and popup lifetime/position edge cases need hands-on Windows verification; Shields still needs `FR-SHD2+` (per-site exceptions, larger lists, YouTube ad handling, tracking-param stripping) and should be implemented with minimal overhead.

## 2026-11-04 Bookmarks: tree structure in `%APPDATA%\featherWIN\bookmarks.txt`, always-visible bar

**Decision:** Bookmarks use a tree structure (BookmarkNode/BookmarkTree) with folders and nesting, saved as UTF-8 with F/B markers and indentation. The bar is always visible under the toolbar (as in Feather), the toolbar has a star button, and Ctrl+D toggles the current page. Click folders to show dropdown menus; add folders from the Bookmarks menu. Drag-and-drop is planned for full implementation.
**Why:** supports Chromium-like folder organization while keeping the format simple and inspectable, matching `FR-SET2`. The tree model allows recursive traversal and flexible reorganization.

## 2026-11-04 Disk cache limit is user-set (250 MB to 5 GB, default 500 MB)

**Decision:** The cache limit is a user setting, as in Feather. 250 MB and 500 MB are the starting points; the default is 500 MB. It fits the low-memory and small-footprint priorities, since the user can lower it.
**Open:** whether Chromium's `--disk-cache-size` switch works through WebView2's `AdditionalBrowserArguments` (test on the Windows VM).

## 2026-11-04 Priorities: ease of use, small install, low memory, above everything else

**Decision:** When two options conflict, pick the one that is easier to use, keeps the install small, and uses less memory. Security is not traded away: it's required, but it's done with the cheapest option that meets it (for example, the Windows built-in SmartScreen rather than a bundled scanner).
**What this means in practice:**
- Memory: tab sleep (`FR-TAB6`, WebView2 `TrySuspend`) moves up the list. Each WebView2 tab is a separate renderer process, so sleeping hidden tabs matters most.
- Install: no bundled engine or big runtime. WebView2 comes from Windows, and the app ships only its own binary.
- Ease of use: the UI stays simple. Extension UI (toolbar, popups, options) is judged by how much it costs to build and maintain, and the popup test decides how far to go.
**Speed rule:** speed wins while memory stays inside an acceptable budget. The budget is lower than Edge's and Chrome's memory use for the same pages. Once a change would bring featherWIN to Edge's or Chrome's memory level, the change gives up speed to save memory.
**Baseline to measure (on the Windows VM):** open the same 6 busy sites in Edge and Chrome, then in featherWIN, and record total memory and time to first page (the Feather README's 6-site test). The budget is set from Edge's number, and it has to be lower.

## 2026-11-04 featherWIN is not a 1:1 port of Feather

**Decision:** Feather's features are the starting point, not a checklist to copy. Security and feature behavior are reworked for Windows where Windows does things differently: SmartScreen and Mark-of-the-Web for downloads, WebView2 security settings, the Recycle Bin rule in AGENTS.md, per-user app data under `%APPDATA%`, and Windows-native dialogs and shortcuts. Each deliberate deviation from the SPEC gets an ADR in `docs/adr/`, per AGENTS.md.
**Why:** The macOS app relies on WebKit, sandboxing and macOS conventions that don't map one-to-one onto WebView2 and Windows. Copying the macOS behavior would mean weaker or odd Windows security.
**Open:** which items the user wants reworked first. Listed in the next session.

## 2026-11-04 Engine is WebView2 (C++), superseding the Servo choice below

**Decision:** featherWIN uses the WebView2 control from C++ (Win32 + WebView2 SDK). The user confirmed this after the downsides were laid out: Chromium, which the Servo entry ruled out, and an early Servo would be harder to ship. Servo is kept as a dropped option.
**Why:** It's Windows-only, so the Chromium runtime is already on the machine, which gives a smaller and faster build. Site compatibility, video and tab suspend are far ahead of Servo. The user found C++ easier to code than Rust here. Full reasoning and consequences: [docs/adr/0001-webview2-engine.md](../docs/adr/0001-webview2-engine.md).
**Extensions:** WebView2 supports loading Chromium extensions (`CoreWebView2Profile.AddBrowserExtensionAsync`, enabled with `AreBrowserExtensionsEnabled`), so `FR-EXT*` is back in scope for Phase 3 with limits. Store install isn't supported. An earlier note in this file said WebView2 had no extension support; that was wrong, and this entry replaces it.
**Open:** verify WebView2 runtime telemetry and update behavior against `C-PRIV` before a release, and test which extensions actually load.

## 2026-11-04 Engine is Servo (not Chromium, not Gecko) — superseded by the WebView2 entry above

**Decision:** featherWIN uses Servo, the Rust browser engine, via its `servo` crate embedding API. The user ruled out Chromium-based engines (WebView2, CEF, Electron). Gecko has no supported desktop embedding API.
**Why:** The user wants no Chromium. Servo is the only embeddable non-Chromium engine. `servo` 0.1.0 is on crates.io (April 2026); monthly releases followed (0.3.0 by June 2026).
**Risks, accepted by the user:**
- Servo has no 1.0 and says production use on arbitrary content needs careful evaluation. Site compatibility is far below Chromium and Gecko. Spike A must measure it on real sites.
- No browser extension support. `FR-EXT*` is dropped for now; SPEC.md must say so.
- Video playback is partial (GStreamer; MP4 compat improved in May 2026). `FR-VID*` and DRM need checking.
- Shields: Servo's resource-load delegate can intercept requests, so the blocklist can be a request filter (`FR-SHD1`).
**Alternatives considered:** WebView2 (Chromium, rejected by the user); CEF and Electron (Chromium, rejected); Firefox fork or Firefox as host (Gecko is large or not standalone).

## 2026-11-04 Engine is Edge WebView2 (Chromium), not Gecko

**Decision:** featherWIN uses the WebView2 control. "Firefox-style" means Firefox's UI and add-on conventions where WebView2 allows, not the Gecko engine.
**Why:** Feather depends on the OS engine (WebKit on macOS). WebView2 is the Windows system web engine and keeps the app small. Gecko can't be embedded in a small app.
**Alternatives considered:** Embedding Gecko (rejected: far too large for a tiny browser); a Chromium build of our own (rejected: not tiny, and the same security-update problem Feather avoids).
**Open:** whether the user accepts Chrome-based add-on support in place of Firefox-only add-ons (see SPEC.md `C-ADDON`).

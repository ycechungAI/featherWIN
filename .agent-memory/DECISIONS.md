# Decisions

Append-only. Newest entry at the top.

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

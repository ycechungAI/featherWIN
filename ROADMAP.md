# Roadmap

Each phase ends with an exit check. Do not start the next phase until the check passes.

## Phase 0: feasibility (spikes)

- Spike A: a Servo window (via the `servo` crate) that loads a page, with a basic address bar. Exit: list 10 common sites with pass/fail, and measure launch time and memory against `NF-1` and `NF-2`.
- Spike B: Shields via Servo's resource-load delegate. Exit: blocks ads on 5 ad-heavy sites; record the approach in DECISIONS.md.
- Spike C: Video. Play an MP4 and an HLS stream in Servo. Exit: works or is documented as not possible (`FR-VID*`).
- Spike D: Windows build. Cross-compile and run on Windows 11 (x64). Exit: the window opens and loads a page.
- Spike E: Downloads save to the Downloads folder with Mark-of-the-Web (`FR-DL2`). Exit: works or documented.
- (Extensions are dropped; no spike.)

## Phase 1: browser core

- Tabs, address bar, search engines, find on page, reopen closed tab, session restore (`FR-TAB*`, `FR-NAV*`).
- History, private windows, settings (`FR-HIS*`, `FR-SAF4`, `FR-SET*`).

## Phase 2: features

- Bookmarks, tab groups, sessions (`FR-BM*`, `FR-GRP*`, `FR-SES1`).
- Shields, HTTPS-only mode, site exceptions (`FR-SHD*`, `FR-SAF1`).
- Downloads panel, video download, Picture in Picture, subtitles (`FR-DL*`, `FR-VID*`).

## Phase 3: extensions

- Install, pin, manage add-ons within WebView2 limits (`FR-EXT*`).

## Phase 4: release

- Installer (`NF-4`), code signing, update path, README and docs site.
- Exit: the feature checklist in SPEC.md is green, or each gap is listed with a reason.

# Current state

_Last updated: 2026-11-04 by OpenCode (Haiku 5.5) when the project was created._

## Done

- Folder created with AGENTS.md, SPEC.md (draft v0.2), ROADMAP.md, README.md, CHANGELOG.md.
- Feather source cloned to `~/dev/feather-src` (from https://github.com/ycechungAI/feather). Read the engine-specific files; findings in SPEC.md section 6.
- Biggest risk: Feather's extensions use WebKit's `WKWebExtension`, which WebView2 doesn't have (`FR-EXT*`). Shields uses `WKContentRuleList`, which also has no WebView2 equivalent.

## Next steps

1. Decide the extensions approach with the user (drop, limit, or change engine). Needed before Phase 3.
2. Phase 0 spikes (ROADMAP.md): WebView2 window (A), Shields filter (B), YouTube ads (C), extension loading (D), Mark-of-the-Web (E).
3. Confirm the Shields approach in DECISIONS.md once Spike B is done.

## Gotchas

- The Feather repo is at https://github.com/ycechungAI/feather. The local `~/dev/Feather` folder does not exist; the clone is `~/dev/feather-src`.
- Gecko (Firefox) engine is out of scope; see SPEC.md `C-ENG`.

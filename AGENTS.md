# AGENTS.md

Guidance for AI coding agents working in this repo.

## What this project is

featherWIN is a Windows-only port of Feather (https://github.com/ycechungAI/feather), a tiny macOS browser that wraps the OS web engine. Target: the same app experience on Windows, using the Servo engine (Rust, `servo` crate embedding API). No Chromium-based engine. Browser extensions are out of scope (see SPEC.md `C-ADDON`).

See [SPEC.md](SPEC.md) for requirements (IDs like `FR-TAB1`), [ROADMAP.md](ROADMAP.md) for the plan, and [docs/](docs/) for decisions.

## Rules

- Windows only. Do not add macOS-only code paths to the main app. Keep any cross-platform helper free of POSIX assumptions.
- Reference requirement IDs (e.g. `FR-TAB1`) in commits, PRs and code comments where a requirement is implemented.
- If an implementation needs to deviate from the spec, update SPEC.md and add an ADR in `docs/adr/` in the same change.
- Preserve Feather's privacy stance: no telemetry, no accounts, no background processes, no network calls of its own (except search suggestions when the user turns them on).
- Never hard-delete user files. Use the Windows Recycle Bin (`IFileOperation` with `FOFX_RECYCLEONDELETE` or `SHFileOperation` with `FOF_ALLOWUNDO`).
- Before finishing a change: build, run the tests, and update `.agent-memory/STATE.md` and `DECISIONS.md`.

## Shared project memory

Before starting work, read `.agent-memory/STATE.md` and `.agent-memory/DECISIONS.md`. When you finish a meaningful chunk of work, update `STATE.md` with what changed and what's next, and append to `DECISIONS.md` for any non-obvious call.

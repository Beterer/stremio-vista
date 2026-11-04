# Repository Guidelines

## Project Structure & Module Organization

This native C TV app keeps playback, networking, account, and stream-selection logic in `src/core/`, screens in `src/screens/`, and rendering and widgets in `src/ui/`. Platform adapters live in `src/platform/webos/` and `src/platform/mac/`. Keep platform-specific APIs in those adapters.

`assets/` holds fonts, translations, artwork, and sounds. `pkg/` holds webOS package metadata. `tools/` contains build, packaging, and diagnostic scripts; `tests/` contains harnesses and fixtures. Vendored dependencies and licenses live in `third_party/`. Generated output belongs in ignored `build/` and `dist/` directories.

## Build, Test, and Development Commands

Run from the repository root in Bash. Scripts default to macOS tool paths; Windows requires a compatible environment and explicit toolchain paths. Configure `ORIGIN_BUILD`, `ZIG`, and `SDK` as needed. The webOS target requires Zig and an ARM webOS SDK; the Mac preview requires SDL2 and OpenGL.

- `bash tools/build.sh webos`: cross-compile the TV executable.
- `bash tools/build.sh mac --run`: build and launch the Mac preview.
- `bash tools/build-engine.sh webos`: build the torrent library.
- `bash tools/build-ffmpeg.sh webos`: build audio-conversion and Dolby Vision support; requires Rust tooling.
- `bash tools/package.sh`: rebuild and create an IPK; requires `ares-package`.
- `bash tools/logs-check.sh`: check log scrubbing and cryptography with a C compiler, zlib, and Node.js.

## Coding Style & Naming Conventions

Match nearby C code: two-space indentation, same-line braces, module-prefixed functions (`best_pick`), and uppercase macros. Keep private helpers `static`. Avoid unrelated reformatting and vendored changes. No formatter or linter is configured; inspect compiler warnings.

## Testing Guidelines

Tests use custom C harnesses and Node.js comparisons; no coverage threshold is configured. Use descriptive JSON fixture names under `tests/best/fixtures/`.

Run `bash tests/best/run.sh <web-app-dir>` for scoring comparisons; the external directory must contain compatible `index.html` and `player.js`. Run `bash tests/dts/run.sh <file.mkv>` with the Mac converter library and FFmpeg installed. For player changes, verify affected behavior on a TV and record the webOS version; distinguish preview results from device validation.

## Commit & Pull Request Guidelines

History uses short descriptive subjects, sometimes scoped, such as `README: Support`. Keep commits focused. PRs should explain behavior, validation, relevant issues, and screenshots for visual changes.

Develop on `custom-player` and push to `origin` (`Beterer/webos`). Keep `main` aligned with `upstream/main`; merge relevant upstream updates into `custom-player`.

## Security & Configuration

Keep credentials outside Git. Optional Trakt/TMDB build keys come from `$ORIGIN_BUILD/keys.env` or `KEYS`; builds work without them. Never commit generated key headers, account tokens, or unsanitized logs.

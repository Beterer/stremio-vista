# Stremio Vista local development on Windows

Use Docker Desktop in **Linux containers** mode. The checked-in development image
provides Node.js 24.19.0, Zig 0.17.0, the webOS TV sysroot, Rust 1.90.0 (including
the ARM soft-float target), GCC, CMake, Ninja, Python, FFmpeg, GDB, clangd,
ShellCheck, and webOS CLI 3.2.6. Zig and SDK archives are checksum-verified.
The SDK archive includes an ARM64-hosted compiler; only its ARM TV sysroot is
used, with the image's x86-64 Zig compiler.

## Everyday commands

Run these from the repository root in PowerShell:

```powershell
.\tools\dev.ps1 setup       # Build/rebuild the development image
.\tools\dev.ps1 doctor      # Check installed tools and the TV sysroot
.\tools\dev.ps1 build       # Build the ARM TV executable
.\tools\dev.ps1 libraries   # Build torrent, audio-conversion, Dolby Vision libraries
.\tools\dev.ps1 test        # Log/crypto, Rust, board/navigation, update, scoring checks
.\tools\dev.ps1 test-media  # Build host converter; test generated DTS media and seeking
.\tools\dev.ps1 preview -Demo  # Open the UI with public catalogs, no account
.\tools\dev.ps1 preview    # Open the UI with your own preview account/profile
.\tools\dev.ps1 preview-stop  # Stop the preview; retain its settings
.\tools\dev.ps1 package     # Rebuild executable and create dist/*.ipk
.\tools\dev.ps1 shell       # Interactive Bash with all tools configured
```

Build the libraries before packaging to include torrent playback and audio
conversion. The first build downloads dependencies and takes considerably longer
than subsequent builds. Source files are mounted directly into `/workspace`;
outputs appear locally in `build/` and `dist/`. Dependency sources and compiler
caches persist in the Docker volume `webos-dev-cache`. Do not delete that volume
unless you intend to rebuild the cached dependencies.

Keep shell scripts and patch files in LF format; `.gitattributes` enforces this for future
checkouts. If Docker is stopped, run `docker desktop start` and retry.

## Interactive UI preview on Windows

Run `.\tools\dev.ps1 preview -Demo`. The launcher builds a separate preview image
on the existing toolchain, compiles the Linux desktop target, and opens
`http://localhost:6080/vnc.html?autoconnect=1&resize=scale` in your browser.
The first build takes longer; subsequent starts reuse the compiled dependencies.
This runs the same native screens and renderer through SDL2, Mesa, and
[noVNC](https://github.com/novnc/noVNC), rather than a separate web UI.
The endpoint is bound to `127.0.0.1`, accessible only from this computer.

Click inside the preview to focus it. Use the mouse or arrow keys to navigate,
Enter to select, Escape/Backspace to go back, and F1-F4 for the TV's colored keys.
The browser keyboard types into text fields. Video playback uses the existing
desktop simulator; no actual films, TV decoding, or sound reach the browser.
Software rendering and browser transport do not reflect television performance.

Demo mode seeds a separate profile with public Cinemeta movie/series catalogs
and requires internet access on first start. Continue Watching is empty without
account history. To sign in to your own account, run `.\tools\dev.ps1 preview`
without `-Demo`; the app opens its normal sign-in screen. Account access and
normal library/progress sync are real. No desktop Stremio credentials are imported.

Settings persist in Docker volumes `stremio-vista-preview-demo` (demo) and
`stremio-vista-preview-data` (your account). These are separate from the TV and
excluded from Git. Restart the command after source changes to rebuild. Stop with
`.\tools\dev.ps1 preview-stop`; stopping retains both profiles. Use `-NoOpen` to
start without opening a browser, or `tools/preview.ps1 -Port 6081` for another port.
Startup problems can be inspected with `docker logs stremio-vista-preview`.

With the preview running, `docker exec stremio-vista-preview bash tests/preview/run.sh`
checks desktop controls, lifecycle, window sizing, and settings persistence.
On Linux with SDL2/OpenGL development packages installed, use
`bash tools/build.sh linux --run` directly. The Mac target shares the same video
simulator, with its own platform and memory adapters.

## Project identity

The fork is [Beterer/stremio-vista](https://github.com/Beterer/stremio-vista);
`upstream` remains [stremio-origin/webos](https://github.com/stremio-origin/webos).
The local folder may remain named `webos`. Vista uses `tv.stremiovista.app`,
while Origin uses `tv.stremioorigin.app`; both can be installed together. The
`stremioorigin` executable name remains an internal build detail. On the TV,
settings, login, logs, and torrent storage are isolated by app ID. Existing Origin data stays with
Origin; sign in separately to Vista. Back up `data/state.json` before reinstalling:
some TVs keep data beside the executable, where an update may replace it. Avoid
uninstalling either app when applying an update.

To create an additional development installation, set
`$env:APPID = 'tv.stremiovista.dev'` before building and packaging.

For phone casting or QR pages, fully exit the other app first: the original and
Vista currently use the same local TCP ports for those optional services.

Update checks target Vista's GitHub releases. The fork's existing `gh-pages` branch
contains inherited Origin packages, not Vista releases; the app does not use that
feed. `tools/release.sh` prepares optional Vista Homebrew metadata for
`https://beterer.github.io/stremio-vista` without publishing it. `SB_UPD_GH` overrides
the release API at runtime; setting a nonempty `SB_UPD_MAN` explicitly opts into a
manifest fallback for development.

The logo source is `assets/vista-logo.png`, with launcher and About variants in
`pkg/vista-icon.png`, `pkg/vista-largeIcon.png`, and `assets/vista-icon.png`.
It was generated with the built-in imagegen tool using the original logo as a
style reference: purple gradient play emblem, solid white spaced `VISTA` lettering,
and an opaque dark background. The variants are resized from that master.
The exact prompt and output paths are in [the logo generation record](vista-logo-prompt.txt).

## Editor

Open this folder in VS Code and select **Dev Containers: Reopen in Container**.
The configuration installs the clangd extension and uses the same source mount
and dependency cache as the PowerShell commands. `compile_flags.txt` supplies TV
headers for C editing; `.clangd` uses host C++20 headers for the engine wrapper.
These fallback editor flags do not change the build. Builds can also be invoked from
the container terminal with `bash tools/build.sh webos`. The browser preview above
uses the `linux` target; the existing `mac` target requires macOS and SDL2.

## Test boundaries

`test` exercises log sealing/scrubbing, the Dolby Vision Rust unit tests, and all
scoring fixtures across four TV capability profiles and five connection speeds.
The board harness checks Continue Watching followed by discovery catalogs, empty
history, stale catalog replies, and the Home/Library navigation layout in both
text directions. It runs the production row/layout code with network and rendering
dependencies stubbed in the configured Linux image using the SDK headers; TV
navigation and appearance still need device verification.
The update harness checks fork-only release lookup, empty releases, network
failures, and explicitly configured manifest fallback without contacting a server.
The metadata harness checks the browsing row's minute duration, year, genres and
IMDb order, missing fields, RTL alignment, and genre updates when focus changes.
Scoring smoke checks validate execution and selection consistency; they do not
replace the independent web-app comparison.

For that comparison, place a compatible external web-app checkout in the ignored
`build/web-reference/` directory, then run `bash tests/best/run.sh build/web-reference` in the development
shell. It needs the external application's `index.html` and `player.js`, which
this repository does not include. Do not commit that checkout.

`test-media` generates synthetic video with DTS audio, converts it through the
host-built library, and runs the existing FFmpeg decode/timestamp/seek harness.
It does not establish TV playback, hardware decoding, or Dolby Vision display
compatibility. Verify player changes on a physical TV and record its webOS version.

## TV access and private settings

The webOS CLI is available in the container. On Windows it can also be installed
with `npm install --global @webos-tools/cli@3.2.6` ([LG instructions](https://webostv.developer.lge.com/develop/tools/cli-installation)).
Use the Windows CLI for TV pairing so its device configuration and keys persist
in your Windows user profile; command containers are disposable.
Configure a TV with `ares-setup-device` and retrieve its Developer Mode SSH key
with `ares-novacom --device <name> --getkey`; the TV must already have Developer
Mode and its key server enabled. Account login and TV pairing require user input.
The existing `tools/tv.sh` assumes rooted SSH access; it is not the Developer
Mode installation path. Use `ares-install` and `ares-launch` for Developer Mode.
CLI 3.2.6 warns that upstream's `pkg/appinfo.json` lacks `requiredACG`.
Packaging succeeds; TV permissions and playback still need device validation.

For a separate development app, set `$env:APPID = 'tv.stremiovista.dev'` and
`$env:TITLE = 'Stremio Vista Dev'` before building/packaging. The default is
`tv.stremiovista.app`; keep Origin's ID reserved for the original app.
Optional Trakt/TMDB keys stay outside Git in `$ORIGIN_BUILD/keys.env` inside the
cache volume. Without keys, the build still works and users supply their own.

Toolchain source: [webOS Homebrew native toolchain](https://github.com/webosbrew/native-toolchain).
The Docker image pins the SDK release and uses [Zig's official downloads](https://ziglang.org/download/).

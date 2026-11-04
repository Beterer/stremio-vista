# Local development on Windows

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
.\tools\dev.ps1 test        # Log/crypto checks, Rust tests, scoring smoke checks
.\tools\dev.ps1 test-media  # Build host converter; test generated DTS media and seeking
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

## Editor

Open this folder in VS Code and select **Dev Containers: Reopen in Container**.
The configuration installs the clangd extension and uses the same source mount
and dependency cache as the PowerShell commands. `compile_flags.txt` supplies TV
headers for C editing; `.clangd` uses host C++20 headers for the engine wrapper.
These fallback editor flags do not change the build. Builds can also be invoked from
the container terminal with `bash tools/build.sh webos`. No Windows or Linux UI
preview target exists; the existing `mac` target requires macOS and SDL2.

## Test boundaries

`test` exercises log sealing/scrubbing, the Dolby Vision Rust unit tests, and all
scoring fixtures across four TV capability profiles and five connection speeds.
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

For a separate custom app, set `$env:APPID = 'tv.beterer.stremio'` and
`$env:TITLE = 'Stremio Custom'` before building/packaging. The defaults retain
upstream's package identity. Choose deliberately before installing on a TV.
Optional Trakt/TMDB keys stay outside Git in `$ORIGIN_BUILD/keys.env` inside the
cache volume. Without keys, the build still works and users supply their own.

Toolchain source: [webOS Homebrew native toolchain](https://github.com/webosbrew/native-toolchain).
The Docker image pins the SDK release and uses [Zig's official downloads](https://ziglang.org/download/).

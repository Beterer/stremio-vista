# Compatibility

These platform expectations and the webOS 4.4 result come from Stremio Origin.
Stremio Vista has been built and packaged locally. Installation alongside Origin
is verified on an LG OLED65C62LA running webOS 11.2.0 (2026), firmware 43.21.98.
Launch, navigation and playback on the TV still need validation.

| webOS | Status |
|---|---|
| 3.x (2016 to 2017 TVs) | Should work, not tested yet |
| 4.x (2018 to 2019 TVs) | Tested upstream on webOS 4.4; Vista pending |
| 5.x and 6.x | Should work, not tested yet |
| 22 to 25 | Should work, not tested yet |
| 26 (11.2.0) | Vista and Origin installed together; launch/playback pending |

## Install
- Developer Mode: build the `.ipk` using [local development](development.md), then install it with webOS Dev Manager or the CLI. Vista uses `tv.stremiovista.app` and can coexist with Origin's `tv.stremioorigin.app`.
- Published Vista packages will appear in [Releases](https://github.com/Beterer/stremio-vista/releases). No Vista Homebrew Channel feed is published yet.

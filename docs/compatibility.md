# Compatibility

These platform expectations and the webOS 4.4 result come from Stremio Origin.
Stremio Vista has been built and packaged locally. Installation alongside Origin
is verified on an LG OLED65C62LA running webOS 11.2.0 (2026), firmware 43.21.98.
Launch, navigation and playback on the TV still need validation.

The original project reports the following compatibility for Origin 1.1.x.

| webOS | TV years | Status |
|---|---|---|
| 3.x | 2016 to 2017 | Didn't start in 1.0.0 (#6). Fixed in 1.1.0. |
| 4.x | 2018 to 2019 | Works. Developed and tested on webOS 4.4, and by several users. |
| 5.x | 2020 | Works, by a user's report. |
| 6.x | 2021 | Works, by users' reports. |
| 22 | 2022 | Works. A crash after playing a film was reported (#25). |
| 23 and 24 | 2023 to 2024 | Should work. No reports yet. |
| 25 | 2025 | Works, by users' reports, live streams included. |

## Install
- Developer Mode: build the `.ipk` using [local development](development.md), then install it with webOS Dev Manager or the CLI. Vista uses `tv.stremiovista.app` and can coexist with Origin's `tv.stremioorigin.app`.
- Published Vista packages will appear in [Releases](https://github.com/Beterer/stremio-vista/releases). No Vista Homebrew Channel feed is published yet.

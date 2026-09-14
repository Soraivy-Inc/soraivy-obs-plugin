# Soraivy for OBS

Native OBS Studio plugin for [Soraivy](https://www.soraivy.com) live streaming — no Python, no copy-paste of server/stream key.

## What it does (v0.1.0 target)

- Adds a **Soraivy** service under OBS Settings → Stream.
- Paste one OBS token (from Soraivy Settings → Streaming), pick RTMPS (30fps) or WHIP (60fps), then Connect / Go Live / End from the plugin panel.
- Tokens are scoped `live:write`-only, revocable, and never logged.

## Platforms

| Platform | Status (v0.1.0) |
|----------|-----------------|
| Windows x64 | NSIS installer, unsigned |
| macOS arm64 + x64 | `.pkg`, signed + notarized |
| Ubuntu x64 | `.deb` |

Requires OBS Studio 31+ (WHIP paths) for full functionality.

## Building

Scaffolded from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate). See its [wiki](https://github.com/obsproject/obs-plugintemplate/wiki) for full build requirements. Quick version:

- Windows: Visual Studio 2022, CMake 3.30+, Qt6 via obs-deps
- macOS: Xcode 16+, CMake 3.30+
- Ubuntu 24.04: CMake 3.28+, ninja, pkg-config, build-essential

`buildspec.json` pins the OBS Studio SDK (31.1.1) and obs-deps. GitHub Actions builds all platforms on push; tags (`0.1.0`, …) produce releases with installers.

## License

GPL-2.0-or-later (see `LICENSE`) — required, as the plugin links OBS `libobs`.

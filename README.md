# MenuTune

**Replace the Geometry Dash main menu music with any song from your Saved Songs collection.**

By **Arsharx7** — a private, local-use [Geode](https://geode-sdk.github.io/) mod for GD 2.2081 (Windows).

## Features

- 🎵 **Pick any Saved Song** — a music-note button on the main menu opens a popup listing every song in your GD Saved Songs library, with title, artist, duration, and song ID.
- 🔍 **Search** — filter your library by name, artist, or ID as you type.
- 🔁 **One-click Default** — restore the vanilla `menuLoop.mp3` menu music at any time.
- 🎯 **Uses GD's own mechanism** — selections ride the game's native custom menu song (`m_customMenuSongID` + `fadeInMenuMusic`), so persistence, volume, gameplay transitions, and fade behavior are all handled by the game itself. No parallel audio pipeline, no modified game files.

## Building

Prerequisites: Visual Studio Build Tools (MSVC), CMake, [Geode SDK](https://github.com/geode-sdk/geode) 5.11.2, Geode CLI.

```powershell
$env:GEODE_SDK = "path\to\geode-sdk"
cmake -S . -B build -A x64
cmake --build build --config Release
```

The packaged `local.menutune.geode` is installed into your GD `geode\mods` folder automatically.

## Notes

- Private local use only — **not** intended for submission to the Geode index.
- Never modifies original game music files.
- Target: Geometry Dash 2.2081, Geode 5.11.2, Windows x64.

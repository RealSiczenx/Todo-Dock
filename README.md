# Todo Dock for OBS Studio

Native OBS task checklist with custom tasks, hotkeys, 24 automatic OBS state conditions, reorder controls, and theme-aware customization.

## Download the Windows DLL

Open [Actions → Build Windows DLL](https://github.com/RealSiczenx/Todo-Dock/actions/workflows/windows.yml), select a successful run, and download **Todo-Dock-Windows-x64** from its Artifacts section. GitHub may require you to sign in to download an Actions artifact.

Extract the ZIP, close OBS, and run `Install.cmd` as administrator. Alternatively, copy `obs-plugins/64bit/task-dock.dll` into that same folder in your OBS installation. Restart OBS and open **Docks → Task Dock**.

Build target: **Windows x64, OBS 31.1.1 SDK, Qt 6**. Compatible newer OBS versions may work, but must be tested. The DLL is unsigned. Native runtime validation inside OBS is separate from successful compilation.

## Features

- **+** → popup to add a custom task or automatic OBS rule.
- **−**, **↑**, **↓** → delete and reorder selected tasks.
- **⚙** → appearance, edit task, hotkey help, reset custom tasks.
- **Custom hotkeys:** OBS Settings → Hotkeys → search `Task Dock`; assign Toggle, Check, and Uncheck independently.
- **Automatic checks:** streaming, recording, pause, replay buffer, virtual camera, Studio Mode, preview, scenes, profiles, collections, transitions, sources, audio mute/volume/monitoring, media playback, filters, and scene-item visibility. Conditions support inversion and refresh every 300 ms.
- **Appearance:** OBS theme, accent color, font size, compact rows, progress bar.
- **Saved state:** task order, manual checkmarks, hotkey bindings, and appearance survive restarts.

Automatic tasks observe state; they do not start or stop outputs. Missing targets remain unchecked, including inverted conditions, except the explicit inverted Source exists condition. Target names must match exactly. Scene visibility checks the item's own eye switch; microphone volume/mute checks do not prove incoming audio. Third-party plugin states and transient actions are not included.

Double-click a task to edit it. The checklist is global across profiles and scene collections. Settings use OBS's `plugin_config/task-dock/tasks.json` location. Close OBS before backing up or resetting that file.

## Building

The Windows workflow checks out official OBS plugin build support pinned to commit `3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc`, overlays this plugin, downloads hash-verified dependencies from `buildspec.json`, builds a Release DLL, runs tests, and uploads an install package. It does not publish a GitHub Release or require repository secrets.

For a local Windows build, reproduce the workflow's prepare/configure/build steps using Visual Studio 2022 x64 and CMake 3.28+. `windows/CMakeLists.txt` is the entry point when overlaid onto the pinned template. The root CMakeLists supports an already-installed OBS/Qt development SDK.

See [docs/TESTING.md](docs/TESTING.md) for the integration checklist.

License: GPL-2.0-or-later. Build support comes from the [official OBS plugin template](https://github.com/obsproject/obs-plugintemplate).

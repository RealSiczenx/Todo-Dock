# Todo Dock for OBS Studio

Native OBS task checklist with custom tasks, hotkeys, **73 automatic native OBS conditions**, reorder controls, and a compact OBS-style dock layout.

## Download the Windows DLL

Open [Actions → Build Windows DLL](https://github.com/RealSiczenx/Todo-Dock/actions/workflows/windows.yml), select a successful run, and download **Todo-Dock-Windows-x64** from its Artifacts section. GitHub may require you to sign in to download an Actions artifact.

Extract the ZIP, close OBS, and run `Install.cmd` as administrator. Alternatively, copy `obs-plugins/64bit/task-dock.dll` into that same folder in your OBS installation. Restart OBS and open **Docks → Task Dock**.

Build target: **Windows x64, OBS 31.1.1 SDK, Qt 6**. Compatible newer OBS versions may work, but must be tested. The DLL is unsigned. Native runtime validation inside OBS is separate from successful compilation.

## Layout

The dock intentionally stays minimal like an OBS utility dock: the task list fills the panel and the bottom toolbar is **Add (+), Delete, Customization (gear), Up, Down**. There is no permanent header or status block taking space away from tasks.

## Features

- **+** → add a custom task or automatic OBS task.
- **Trash** → delete the selected task.
- **⚙** → customization, edit task, hotkey help, reset manual checks.
- **↑ / ↓** → reorder the selected task.
- **Custom hotkeys:** OBS Settings → Hotkeys → search `Task Dock`; assign Toggle, Check, and Uncheck independently.
- **Automatic checks:** 73 native conditions covering outputs, streaming, recording, replay buffer, virtual camera, Studio Mode, preview, produced files, streaming service, scenes, profiles, collections, transitions, canvases, source existence/enabled/active/showing/capabilities, audio state, monitoring, push-to-mute/talk, force mono, async modes, filters, audio tracks 1–6, all OBS media states, filter existence/enabled state, and scene-item existence/visibility/lock/selection/group/show-hide transitions.
- **Appearance:** follows the OBS theme, with configurable accent, text size, and compact rows.
- **Saved state:** task order, manual checkmarks, hotkey bindings, and appearance survive restarts.

Automatic tasks observe state; they do not start or stop outputs. Missing required targets stay unchecked, including inverted conditions, except explicit existence rules such as Source exists, Filter exists, Scene item exists, and Canvas exists. Target names must match exactly.

Double-click a task to edit it. The checklist is global across profiles and scene collections. Settings use OBS's `plugin_config/task-dock/tasks.json` location. Close OBS before backing up or resetting that file.

## Building

The Windows workflow checks out official OBS plugin build support pinned to commit `3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc`, overlays this plugin, downloads hash-verified dependencies from `buildspec.json`, builds a Release DLL, runs tests, and uploads an install package.

For a local Windows build, reproduce the workflow's prepare/configure/build steps using Visual Studio 2022 x64 and CMake 3.28+. `windows/CMakeLists.txt` is the entry point when overlaid onto the pinned template. The root CMakeLists supports an already-installed OBS/Qt development SDK.

See [docs/TESTING.md](docs/TESTING.md) for the integration checklist.

License: GPL-2.0-or-later. Build support comes from the [official OBS plugin template](https://github.com/obsproject/obs-plugintemplate).

# Validation record

## Portable logic checks

The portable C++ logic tests cover condition inversion, missing targets, and reorder boundaries.

Local validation command:

    cmake -S . -B build-tests -DTASK_DOCK_TESTS_ONLY=ON
    cmake --build build-tests
    ctest --test-dir build-tests --output-on-failure

Result in the preparation environment: **12 logic checks passed**.

## Native integration checklist

A successful GitHub Actions compile confirms the Windows translation unit and link step against the pinned OBS 31.1.1 / Qt 6 dependency set. It does not replace runtime testing in OBS.

Before treating a binary as production-ready:

1. Build and load the DLL, then confirm the OBS log contains `[task-dock] Loaded v1.0.0`.
2. Show **Docks → Task Dock**, resize, float, re-dock, close/reopen, and restart OBS.
3. Create/edit/delete/reorder custom tasks; verify check states save across restart.
4. Assign Toggle, Check, and Uncheck hotkeys and confirm they remain attached after rename/reorder/restart.
5. Exercise streaming, recording, pause, replay buffer, virtual camera, Studio Mode, preview, scene/profile/collection/transition rules.
6. Exercise source active/showing, mute/volume/monitoring, media playback, filters, and scene-item visibility.
7. Rename/remove rule targets and confirm missing targets stay unchecked.
8. Test appearance at multiple DPI/scaling settings and with long/Unicode task names.
9. Corrupt or make the configuration file read-only and confirm failures are visible without destroying the old file.
10. Close OBS with the dock visible, hidden, and floating; check for crashes, hangs, dangling callbacks, or lost saves.

Record the OBS version, Windows version, Qt version, compiler, and test results alongside any distributed binary.

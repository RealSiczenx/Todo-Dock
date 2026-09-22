#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <util/platform.h>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QFormLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSize>
#include <QStyle>
#include <QToolBar>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <vector>
#include "logic.hpp"

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description(void)
{
    return "Task Dock: custom checklists, native OBS automatic conditions and hotkeys";
}
MODULE_EXPORT const char *obs_module_name(void) { return "Task Dock"; }
MODULE_EXPORT const char *obs_module_author(void) { return "Task Dock contributors"; }

namespace {
constexpr const char *DockId = "com.taskdock.checklist";
class TaskDock;
QPointer<TaskDock> dock;
bool exiting = false;

struct Rule { const char *id; const char *name; int target; };
// target: 0 none, 1 source, 2 scene, 3 profile, 4 collection, 5 transition,
// 6 source + filter, 7 scene + scene item, 8 canvas.
constexpr Rule rules[] = {
    {"streaming", "Outputs • Streaming is active", 0},
    {"stream_reconnecting", "Outputs • Streaming is reconnecting", 0},
    {"stream_delay", "Outputs • Streaming delay is active", 0},
    {"stream_data", "Outputs • Streaming is active and sending data", 0},
    {"recording", "Outputs • Recording is active", 0},
    {"recording_running", "Outputs • Recording is active and not paused", 0},
    {"paused", "Outputs • Recording is paused", 0},
    {"replay", "Outputs • Replay buffer is active", 0},
    {"virtualcam", "Outputs • Virtual camera is active", 0},
    {"studio", "Interface • Studio Mode is enabled", 0},
    {"preview", "Interface • Preview is enabled", 0},
    {"dark_theme", "Interface • OBS dark theme is active", 0},
    {"last_recording", "Files • A recording has been produced", 0},
    {"last_replay", "Files • A replay has been saved", 0},
    {"last_screenshot", "Files • A screenshot has been taken", 0},
    {"record_path", "Files • Recording output path is configured", 0},
    {"service_configured", "Streaming • A streaming service is configured", 0},

    {"scene", "Scenes • Program scene matches", 2},
    {"preview_scene", "Scenes • Studio preview scene matches", 2},
    {"profile", "Scenes • Profile matches", 3},
    {"collection", "Scenes • Scene collection matches", 4},
    {"transition", "Scenes • Selected transition matches", 5},
    {"transition_running", "Scenes • A transition is currently running", 0},
    {"transition_fixed", "Scenes • Selected transition has fixed duration", 0},
    {"canvas_exists", "Canvases • Canvas exists", 8},
    {"canvas_video", "Canvases • Canvas has active video", 8},

    {"exists", "Sources • Source exists", 1},
    {"enabled", "Sources • Source is enabled", 1},
    {"active", "Sources • Source is active in program output", 1},
    {"showing", "Sources • Source is showing anywhere", 1},
    {"hidden", "Sources • Source is hidden from searches", 1},
    {"has_video", "Sources • Source has video capability", 1},
    {"has_audio", "Sources • Source has audio capability", 1},
    {"interactive", "Sources • Source supports interaction", 1},
    {"controllable_media", "Sources • Source supports media controls", 1},
    {"audio_active", "Audio • Source audio is active", 1},
    {"muted", "Audio • Source is muted", 1},
    {"unmuted", "Audio • Source is unmuted", 1},
    {"audible", "Audio • Source is unmuted with volume above zero", 1},
    {"monitor_off", "Audio • Monitoring is off", 1},
    {"monitor_only", "Audio • Monitoring is Monitor Only", 1},
    {"monitor_output", "Audio • Monitoring is Monitor and Output", 1},
    {"push_to_mute", "Audio • Push-to-mute is enabled", 1},
    {"push_to_talk", "Audio • Push-to-talk is enabled", 1},
    {"force_mono", "Audio • Force mono is enabled", 1},
    {"async_unbuffered", "Sources • Async unbuffered mode is enabled", 1},
    {"async_decoupled", "Sources • Async decoupled mode is enabled", 1},
    {"filters_present", "Filters • Source has at least one filter", 1},
    {"track1", "Audio • Source is routed to Track 1", 1},
    {"track2", "Audio • Source is routed to Track 2", 1},
    {"track3", "Audio • Source is routed to Track 3", 1},
    {"track4", "Audio • Source is routed to Track 4", 1},
    {"track5", "Audio • Source is routed to Track 5", 1},
    {"track6", "Audio • Source is routed to Track 6", 1},

    {"media_none", "Media • Media state is None", 1},
    {"playing", "Media • Media is playing", 1},
    {"media_opening", "Media • Media is opening", 1},
    {"media_buffering", "Media • Media is buffering", 1},
    {"media_paused", "Media • Media is paused", 1},
    {"media_stopped", "Media • Media is stopped", 1},
    {"media_ended", "Media • Media has ended", 1},
    {"media_error", "Media • Media is in error state", 1},

    {"filter_exists", "Filters • Named filter exists", 6},
    {"filter", "Filters • Named filter is enabled", 6},

    {"item_exists", "Scene items • Named item exists", 7},
    {"visible", "Scene items • Item visibility is on", 7},
    {"item_locked", "Scene items • Item is locked", 7},
    {"item_selected", "Scene items • Item is selected", 7},
    {"item_group", "Scene items • Item is a group", 7},
    {"item_show_transition", "Scene items • Item has a show transition", 7},
    {"item_hide_transition", "Scene items • Item has a hide transition", 7},
    {"item_show_duration", "Scene items • Show transition duration is above zero", 7},
    {"item_hide_duration", "Scene items • Hide transition duration is above zero", 7}
};

const Rule *findRule(const QString &id)
{
    for (const auto &r : rules) if (id == r.id) return &r;
    return nullptr;
}

struct Task {
    QString id, title, rule = "custom", target, detail;
    bool checked = false, inverted = false;
    QJsonArray bindings[3];
};
struct Binding {
    TaskDock *owner;
    QString taskId;
    int mode;
    obs_hotkey_id hotkey = OBS_INVALID_HOTKEY_ID;
};
struct Observation { std::optional<bool> value; QString message; };

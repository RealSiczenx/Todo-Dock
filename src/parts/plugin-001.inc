#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <util/platform.h>
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
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
    return "Task Dock: custom checklists, OBS status rules and native hotkeys";
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
// 6 source + filter, 7 scene + source.
constexpr Rule rules[] = {
    {"streaming", "Streaming is active", 0},
    {"recording", "Recording is active", 0},
    {"recording_running", "Recording is active and not paused", 0},
    {"paused", "Recording is paused", 0},
    {"replay", "Replay buffer is active", 0},
    {"virtualcam", "Virtual camera is active", 0},
    {"studio", "Studio mode is enabled", 0},
    {"preview", "Preview is enabled", 0},
    {"scene", "Program scene matches", 2},
    {"preview_scene", "Studio preview scene matches", 2},
    {"profile", "Profile matches", 3},
    {"collection", "Scene collection matches", 4},
    {"transition", "Selected transition matches", 5},
    {"exists", "Source exists", 1},
    {"active", "Source is active in the output", 1},
    {"showing", "Source is showing (including previews)", 1},
    {"muted", "Audio source is muted", 1},
    {"audible", "Audio source is unmuted with volume above zero", 1},
    {"monitor", "Audio monitoring is enabled", 1},
    {"playing", "Media source is playing", 1},
    {"media_paused", "Media source is paused", 1},
    {"media_ended", "Media source has ended", 1},
    {"filter", "Source filter is enabled", 6},
    {"visible", "Scene item visibility is on", 7}
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

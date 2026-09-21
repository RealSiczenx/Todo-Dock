QString sourceName(obs_source_t *source)
{
    return source ? QString::fromUtf8(obs_source_get_name(source)) : QString();
}
Observation observe(const Task &t)
{
    const auto &k = t.rule;
    if (k == "streaming") return {obs_frontend_streaming_active(), {}};
    if (k == "recording") return {obs_frontend_recording_active(), {}};
    if (k == "recording_running") return {obs_frontend_recording_active() && !obs_frontend_recording_paused(), {}};
    if (k == "paused") return {obs_frontend_recording_active() && obs_frontend_recording_paused(), {}};
    if (k == "replay") return {obs_frontend_replay_buffer_active(), {}};
    if (k == "virtualcam") return {obs_frontend_virtualcam_active(), {}};
    if (k == "studio") return {obs_frontend_preview_program_mode_active(), {}};
    if (k == "preview") return {obs_frontend_preview_enabled(), {}};
    if (k == "scene" || k == "preview_scene" || k == "transition") {
        if (k == "preview_scene" && !obs_frontend_preview_program_mode_active())
            return {std::nullopt, "Enable Studio Mode to monitor a preview scene"};
        obs_frontend_source_list available = {};
        if (k == "transition") obs_frontend_get_transitions(&available);
        else obs_frontend_get_scenes(&available);
        bool targetExists = false;
        for (size_t i = 0; i < available.sources.num; ++i)
            if (sourceName(available.sources.array[i]) == t.target) targetExists = true;
        obs_frontend_source_list_free(&available);
        if (!targetExists) return {std::nullopt, "Target is missing: " + t.target};
        auto *s = k == "scene" ? obs_frontend_get_current_scene() :
                  k == "preview_scene" ? obs_frontend_get_current_preview_scene() : obs_frontend_get_current_transition();
        const bool same = sourceName(s) == t.target;
        const bool exists = s != nullptr;
        if (s) obs_source_release(s);
        return exists ? Observation{same, {}} : Observation{std::nullopt, "No current scene or transition"};
    }
    if (k == "profile" || k == "collection") {
        char **available = k == "profile" ? obs_frontend_get_profiles() : obs_frontend_get_scene_collections();
        bool targetExists = false;
        if (available) {
            for (char **n = available; *n; ++n) if (QString::fromUtf8(*n) == t.target) targetExists = true;
            bfree(available);
        }
        if (!targetExists) return {std::nullopt, "Target is missing: " + t.target};
        char *name = k == "profile" ? obs_frontend_get_current_profile() : obs_frontend_get_current_scene_collection();
        if (!name) return {std::nullopt, "No current profile or collection"};
        const bool same = QString::fromUtf8(name) == t.target;
        bfree(name);
        return {same, {}};
    }
    obs_source_t *s = obs_get_source_by_name(t.target.toUtf8().constData());
    if (k == "exists") {
        if (s) obs_source_release(s);
        return {s != nullptr, {}};
    }
    if (!s) return {std::nullopt, "Target is missing: " + t.target};
    Observation result{std::nullopt, "Unsupported rule"};
    const uint32_t flags = obs_source_get_output_flags(s);
    if (k == "active") result = {obs_source_active(s), {}};
    else if (k == "showing") result = {obs_source_showing(s), {}};
    else if (k == "muted" || k == "audible" || k == "monitor") {
        if (!(flags & OBS_SOURCE_AUDIO)) result = {std::nullopt, "Choose an audio source"};
        else if (k == "muted") result = {obs_source_muted(s), {}};
        else if (k == "audible") result = {!obs_source_muted(s) && obs_source_get_volume(s) > 0.0f, {}};
        else result = {obs_source_get_monitoring_type(s) != OBS_MONITORING_TYPE_NONE, {}};
    } else if (k == "playing" || k == "media_paused" || k == "media_ended") {
        if (!(flags & OBS_SOURCE_CONTROLLABLE_MEDIA)) result = {std::nullopt, "Choose a controllable media source"};
        else {
            const auto state = obs_source_media_get_state(s);
            result = {state == (k == "playing" ? OBS_MEDIA_STATE_PLAYING :
                              k == "media_paused" ? OBS_MEDIA_STATE_PAUSED : OBS_MEDIA_STATE_ENDED), {}};
        }
    } else if (k == "filter") {
        obs_source_t *filter = obs_source_get_filter_by_name(s, t.detail.toUtf8().constData());
        result = filter ? Observation{obs_source_enabled(filter), {}} : Observation{std::nullopt, "Filter is missing: " + t.detail};
        if (filter) obs_source_release(filter);
    } else if (k == "visible") {
        auto *scene = obs_scene_from_source(s);
        auto *item = scene ? obs_scene_find_source_recursive(scene, t.detail.toUtf8().constData()) : nullptr;
        result = item ? Observation{obs_sceneitem_visible(item), {}} : Observation{std::nullopt, "Scene item is missing: " + t.detail};
    }
    obs_source_release(s);
    return result;
}

class TaskDock final : public QWidget {
    std::vector<Task> tasks;
    std::vector<std::unique_ptr<Binding>> hotkeys;
    QListWidget *list;
    QLabel *subtitle, *empty, *saveStatus;
    QProgressBar *progress;
    QToolButton *removeButton, *upButton, *downButton;
    QTimer timer;
    QString configPath, accent = "#7C9CFF";
    int fontSize = 13;
    bool compact = false, showProgress = true, stopped = false, canSave = true;

    static QToolButton *button(const QString &text, const QString &tip, QWidget *parent)
    {
        auto *b = new QToolButton(parent);
        b->setText(text); b->setToolTip(tip); b->setAccessibleName(tip);
        b->setMinimumSize(32, 30);
        return b;

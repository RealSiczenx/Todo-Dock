QString sourceName(obs_source_t *source)
{
    return source ? QString::fromUtf8(obs_source_get_name(source)) : QString();
}

Observation frontendPathAvailable(char *(*getter)(void))
{
    char *path = getter();
    const bool available = path && *path;
    if (path) bfree(path);
    return {available, {}};
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
    if (k == "dark_theme") return {obs_frontend_is_theme_dark(), {}};
    if (k == "last_recording") return frontendPathAvailable(obs_frontend_get_last_recording);
    if (k == "last_replay") return frontendPathAvailable(obs_frontend_get_last_replay);
    if (k == "last_screenshot") return frontendPathAvailable(obs_frontend_get_last_screenshot);
    if (k == "record_path") return frontendPathAvailable(obs_frontend_get_current_record_output_path);
    if (k == "service_configured") return {obs_frontend_get_streaming_service() != nullptr, {}};

    if (k == "stream_reconnecting" || k == "stream_delay" || k == "stream_data") {
        obs_output_t *output = obs_frontend_get_streaming_output();
        if (!output) return {std::nullopt, "Streaming output is unavailable"};
        bool value = false;
        if (k == "stream_reconnecting") value = obs_output_reconnecting(output);
        else if (k == "stream_delay") value = obs_output_get_active_delay(output) > 0;
        else value = obs_frontend_streaming_active() && obs_output_get_total_bytes(output) > 0;
        obs_output_release(output);
        return {value, {}};
    }

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
                  k == "preview_scene" ? obs_frontend_get_current_preview_scene() :
                  obs_frontend_get_current_transition();
        const bool same = sourceName(s) == t.target;
        const bool existsNow = s != nullptr;
        if (s) obs_source_release(s);
        return existsNow ? Observation{same, {}} : Observation{std::nullopt, "No current scene or transition"};
    }

    if (k == "transition_running" || k == "transition_fixed") {
        obs_source_t *transition = obs_frontend_get_current_transition();
        if (!transition) return {std::nullopt, "No selected transition"};
        bool value = false;
        if (k == "transition_fixed") value = obs_transition_fixed(transition);
        else {
            const float time = obs_transition_get_time(transition);
            value = time > 0.0f && time < 1.0f;
        }
        obs_source_release(transition);
        return {value, {}};
    }

    if (k == "profile" || k == "collection") {
        char **available = k == "profile" ? obs_frontend_get_profiles() : obs_frontend_get_scene_collections();
        bool targetExists = false;
        if (available) {
            for (char **n = available; *n; ++n)
                if (QString::fromUtf8(*n) == t.target) targetExists = true;
            bfree(available);
        }
        if (!targetExists) return {std::nullopt, "Target is missing: " + t.target};

        char *name = k == "profile" ? obs_frontend_get_current_profile() : obs_frontend_get_current_scene_collection();
        if (!name) return {std::nullopt, "No current profile or collection"};
        const bool same = QString::fromUtf8(name) == t.target;
        bfree(name);
        return {same, {}};
    }

    if (k == "canvas_exists" || k == "canvas_video") {
        obs_canvas_t *canvas = obs_get_canvas_by_name(t.target.toUtf8().constData());
        if (k == "canvas_exists") {
            const bool existsNow = canvas != nullptr;
            if (canvas) obs_canvas_release(canvas);
            return {existsNow, {}};
        }
        if (!canvas) return {std::nullopt, "Canvas is missing: " + t.target};
        const bool hasVideo = obs_canvas_has_video(canvas);
        obs_canvas_release(canvas);
        return {hasVideo, {}};
    }

    obs_source_t *s = obs_get_source_by_name(t.target.toUtf8().constData());
    if (k == "exists") {
        const bool existsNow = s != nullptr;
        if (s) obs_source_release(s);
        return {existsNow, {}};
    }
    if (!s) return {std::nullopt, "Target is missing: " + t.target};

    Observation result{std::nullopt, "Unsupported rule"};
    const uint32_t flags = obs_source_get_output_flags(s);
    const uint32_t sourceFlags = obs_source_get_flags(s);

    if (k == "enabled") result = {obs_source_enabled(s), {}};
    else if (k == "active") result = {obs_source_active(s), {}};
    else if (k == "showing") result = {obs_source_showing(s), {}};
    else if (k == "hidden") result = {obs_source_is_hidden(s), {}};
    else if (k == "has_video") result = {(flags & OBS_SOURCE_VIDEO) != 0, {}};
    else if (k == "has_audio") result = {(flags & OBS_SOURCE_AUDIO) != 0, {}};
    else if (k == "interactive") result = {(flags & OBS_SOURCE_INTERACTION) != 0, {}};
    else if (k == "controllable_media") result = {(flags & OBS_SOURCE_CONTROLLABLE_MEDIA) != 0, {}};
    else if (k == "async_unbuffered") result = {obs_source_async_unbuffered(s), {}};
    else if (k == "async_decoupled") result = {obs_source_async_decoupled(s), {}};
    else if (k == "filters_present") result = {obs_source_filter_count(s) > 0, {}};
    else if (k == "audio_active" || k == "muted" || k == "unmuted" || k == "audible" ||
             k == "monitor_off" || k == "monitor_only" || k == "monitor_output" ||
             k == "push_to_mute" || k == "push_to_talk" || k == "force_mono" ||
             k.startsWith("track")) {
        if (!(flags & OBS_SOURCE_AUDIO)) {
            result = {std::nullopt, "Choose an audio source"};
        } else if (k == "audio_active") result = {obs_source_audio_active(s), {}};
        else if (k == "muted") result = {obs_source_muted(s), {}};
        else if (k == "unmuted") result = {!obs_source_muted(s), {}};
        else if (k == "audible") result = {!obs_source_muted(s) && obs_source_get_volume(s) > 0.0f, {}};
        else if (k == "monitor_off") result = {obs_source_get_monitoring_type(s) == OBS_MONITORING_TYPE_NONE, {}};
        else if (k == "monitor_only") result = {obs_source_get_monitoring_type(s) == OBS_MONITORING_TYPE_MONITOR_ONLY, {}};
        else if (k == "monitor_output") result = {obs_source_get_monitoring_type(s) == OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT, {}};
        else if (k == "push_to_mute") result = {obs_source_push_to_mute_enabled(s), {}};
        else if (k == "push_to_talk") result = {obs_source_push_to_talk_enabled(s), {}};
        else if (k == "force_mono") result = {(sourceFlags & OBS_SOURCE_FLAG_FORCE_MONO) != 0, {}};
        else {
            bool ok = false;
            const int track = k.mid(5).toInt(&ok);
            result = ok && track >= 1 && track <= 6
                ? Observation{(obs_source_get_audio_mixers(s) & (1u << (track - 1))) != 0, {}}
                : Observation{std::nullopt, "Invalid audio track rule"};
        }
    } else if (k.startsWith("media_") || k == "playing") {
        if (!(flags & OBS_SOURCE_CONTROLLABLE_MEDIA)) {
            result = {std::nullopt, "Choose a controllable media source"};
        } else {
            const auto state = obs_source_media_get_state(s);
            enum obs_media_state wanted = OBS_MEDIA_STATE_NONE;
            if (k == "playing") wanted = OBS_MEDIA_STATE_PLAYING;
            else if (k == "media_opening") wanted = OBS_MEDIA_STATE_OPENING;
            else if (k == "media_buffering") wanted = OBS_MEDIA_STATE_BUFFERING;
            else if (k == "media_paused") wanted = OBS_MEDIA_STATE_PAUSED;
            else if (k == "media_stopped") wanted = OBS_MEDIA_STATE_STOPPED;
            else if (k == "media_ended") wanted = OBS_MEDIA_STATE_ENDED;
            else if (k == "media_error") wanted = OBS_MEDIA_STATE_ERROR;
            result = {state == wanted, {}};
        }
    } else if (k == "filter_exists" || k == "filter") {
        obs_source_t *filter = obs_source_get_filter_by_name(s, t.detail.toUtf8().constData());
        if (k == "filter_exists") {
            result = {filter != nullptr, {}};
        } else {
            result = filter ? Observation{obs_source_enabled(filter), {}}
                            : Observation{std::nullopt, "Filter is missing: " + t.detail};
        }
        if (filter) obs_source_release(filter);
    } else if (k.startsWith("item_") || k == "visible") {
        auto *scene = obs_scene_from_source(s);
        if (!scene) {
            result = {std::nullopt, "Choose a scene"};
        } else {
            auto *item = obs_scene_find_source_recursive(scene, t.detail.toUtf8().constData());
            if (k == "item_exists") {
                result = {item != nullptr, {}};
            } else if (!item) {
                result = {std::nullopt, "Scene item is missing: " + t.detail};
            } else if (k == "visible") result = {obs_sceneitem_visible(item), {}};
            else if (k == "item_locked") result = {obs_sceneitem_locked(item), {}};
            else if (k == "item_selected") result = {obs_sceneitem_selected(item), {}};
            else if (k == "item_group") result = {obs_sceneitem_is_group(item), {}};
            else if (k == "item_show_duration") result = {obs_sceneitem_get_transition_duration(item, true) > 0, {}};
            else if (k == "item_hide_duration") result = {obs_sceneitem_get_transition_duration(item, false) > 0, {}};
            else if (k == "item_show_transition" || k == "item_hide_transition") {
                obs_source_t *transition = obs_sceneitem_get_transition(item, k == "item_show_transition");
                result = {transition != nullptr, {}};
                if (transition) obs_source_release(transition);
            }
        }
    }

    obs_source_release(s);
    return result;
}

class TaskDock final : public QWidget {
    std::vector<Task> tasks;
    std::vector<std::unique_ptr<Binding>> hotkeys;
    QListWidget *list;
    QLabel *streamStatus, *saveStatus;
    QAction *removeButton, *upButton, *downButton;
    QString clickedSelectionId;
    QTimer timer;
    QString configPath, accent = "#7C9CFF";
    int fontSize = 13;
    bool compact = false, stopped = false, canSave = true;

    static QAction *makeAction(const QString &text, const QString &tip,
                               const char *iconClass, QObject *parent)
    {
        // Do not assign a fixed icon here. OBS themes supply the correct
        // Dark/Light/System icon through the dynamic class property.
        auto *action = new QAction(text, parent);
        action->setToolTip(tip);
        action->setProperty("class", iconClass);
        return action;
    }

    static void applyObsIconClass(QToolBar *toolbar, QAction *action, const char *iconClass)
    {
        action->setProperty("class", iconClass);
        if (auto *widget = toolbar->widgetForAction(action)) {
            widget->setProperty("class", iconClass);
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
        }

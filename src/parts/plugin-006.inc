            QString tip = "Custom task • Click the checkbox or use an OBS hotkey";

            if (t.rule != "custom") {
                auto observation = findRule(t.rule)
                    ? observe(t)
                    : Observation{std::nullopt, "Unknown rule. Edit this task."};

                t.checked = taskdock::completed(observation.value, t.inverted);
                const auto *r = findRule(t.rule);

                tip = observation.message.isEmpty()
                    ? QString("Automatic • %1%2%3%4")
                        .arg(t.inverted ? "NOT: " : "",
                             r ? QString::fromUtf8(r->name) : "Unknown",
                             t.target.isEmpty() ? "" : " • " + t.target,
                             t.detail.isEmpty() ? "" : " • " + t.detail)
                    : observation.message;

                item->setText(t.title);
            } else {
                item->setText(t.title);
            }

            item->setCheckState(t.checked ? Qt::Checked : Qt::Unchecked);
            item->setToolTip(tip);

            QFont f = list->font();
            f.setStrikeOut(t.checked);
            item->setFont(f);
        }

        int incomplete = 0;
        for (const auto &task : tasks)
            if (!task.checked)
                ++incomplete;

        const bool live = obs_frontend_streaming_active();
        if (live) {
            if (incomplete == 0) {
                streamStatus->setText("You are live");
            } else {
                streamStatus->setText(
                    incomplete == 1
                        ? "Live But 1 Task Incomplete"
                        : QString("Live But %1 Tasks Incomplete").arg(incomplete));
            }
        } else {
            streamStatus->setText(
                incomplete == 0 ? "You are ready to stream"
                                : "You are not ready to stream");
        }
    }

    void persist()
    {
        if (!stopped)
            save();
    }

    void shutdown()
    {
        if (stopped) return;
        timer.stop();
        save();
        unregisterHotkeys();
        stopped = true;
    }
};

void frontendEvent(enum obs_frontend_event event, void *)
{
    if (event == OBS_FRONTEND_EVENT_EXIT) {
        if (dock)
            dock->shutdown();
        exiting = true;
    } else if (!exiting && dock) {
        QMetaObject::invokeMethod(dock.data(), [] {
            if (dock)
                dock->refresh();
        }, Qt::QueuedConnection);
    }
}

void frontendSave(obs_data_t *, bool saving, void *)
{
    if (saving && !exiting && dock)
        dock->persist();
}
}

bool obs_module_load(void)
{
    return true;
}

void obs_module_post_load(void)
{
    dock = new TaskDock;
    if (!obs_frontend_add_dock_by_id(DockId, "Task Dock", dock.data())) {
        delete dock.data();
        dock = nullptr;
        blog(LOG_ERROR, "[task-dock] Could not register dock");
        return;
    }

    obs_frontend_add_event_callback(frontendEvent, nullptr);
    obs_frontend_add_save_callback(frontendSave, nullptr);
    blog(LOG_INFO, "[task-dock] Loaded v1.2.2 with 73 automatic conditions");
}

void obs_module_unload(void)
{
    if (!exiting) {
        obs_frontend_remove_event_callback(frontendEvent, nullptr);
        obs_frontend_remove_save_callback(frontendSave, nullptr);
        if (dock) {
            dock->shutdown();
            obs_frontend_remove_dock(DockId);
        }
    } else if (dock) {
        delete dock.data();
    }
}

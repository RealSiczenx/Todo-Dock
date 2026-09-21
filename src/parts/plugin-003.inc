    }
    Task *byId(const QString &id)
    {
        for (auto &t : tasks) if (t.id == id) return &t;
        return nullptr;
    }
    static void onHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
    {
        if (!pressed) return;
        const auto *b = static_cast<Binding *>(data);
        auto *owner = b->owner;
        const auto id = b->taskId;
        const auto mode = b->mode;
        QMetaObject::invokeMethod(owner, [owner, id, mode] {
            if (owner->stopped) return;
            if (auto *t = owner->byId(id); t && t->rule == "custom") {
                t->checked = mode == 0 ? !t->checked : mode == 1;
                owner->refresh(); owner->save();
            }
        }, Qt::QueuedConnection);
    }
    void captureHotkeys()
    {
        for (const auto &b : hotkeys) if (auto *t = byId(b->taskId)) {
            auto *array = obs_hotkey_save(b->hotkey);
            auto *wrapper = obs_data_create();
            obs_data_set_array(wrapper, "keys", array);
            const char *json = obs_data_get_json(wrapper);
            t->bindings[b->mode] = QJsonDocument::fromJson(json ? QByteArray(json) : QByteArray()).object()["keys"].toArray();
            obs_data_release(wrapper); obs_data_array_release(array);
        }
    }
    void unregisterHotkeys()
    {
        for (auto &b : hotkeys) if (b->hotkey != OBS_INVALID_HOTKEY_ID) obs_hotkey_unregister(b->hotkey);
        hotkeys.clear();
    }
    void registerTask(Task &t)
    {
        if (t.rule != "custom") return;
        const std::array<QString, 3> actions{"Toggle", "Check", "Uncheck"};
        for (int mode = 0; mode < 3; ++mode) {
            auto b = std::make_unique<Binding>();
            b->owner = this; b->taskId = t.id; b->mode = mode;
            const auto name = QString("taskdock.%1.%2").arg(t.id).arg(mode).toUtf8();
            const auto label = QString("Task Dock • %1 • %2").arg(t.title, actions[mode]).toUtf8();
            b->hotkey = obs_hotkey_register_frontend(name.constData(), label.constData(), onHotkey, b.get());
            if (b->hotkey == OBS_INVALID_HOTKEY_ID) continue;
            const QByteArray json = QJsonDocument(QJsonObject{{"keys", t.bindings[mode]}}).toJson(QJsonDocument::Compact);
            auto *wrapper = obs_data_create_from_json(json.constData());
            auto *array = obs_data_get_array(wrapper, "keys");
            obs_hotkey_load(b->hotkey, array);
            obs_data_array_release(array); obs_data_release(wrapper);
            hotkeys.push_back(std::move(b));
        }
    }
    void load()
    {
        char *path = obs_module_config_path("tasks.json");
        if (!path) { canSave = false; return; }
        configPath = QString::fromUtf8(path); bfree(path);
        QFile f(configPath);
        if (!f.exists()) return;
        if (!f.open(QIODevice::ReadOnly)) { canSave = false; return; }
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(f.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject() || doc.object()["version"].toInt() != 1 || !doc.object()["tasks"].isArray()) {
            canSave = false; return;
        }
        const auto o = doc.object();
        const auto color = o["accent"].toString(accent);
        if (QColor(color).isValid()) accent = QColor(color).name();
        compact = o["compact"].toBool(); showProgress = o["showProgress"].toBool(true);
        fontSize = std::clamp(o["fontSize"].toInt(13), 10, 24);
        for (const auto v : o["tasks"].toArray()) {
            const auto j = v.toObject();
            Task t;
            t.id = j["id"].toString(); t.title = j["title"].toString().trimmed();
            if (t.title.isEmpty()) continue;
            if (t.id.isEmpty() || byId(t.id)) t.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            t.rule = j["rule"].toString("custom"); t.target = j["target"].toString(); t.detail = j["detail"].toString();
            t.checked = j["checked"].toBool(); t.inverted = j["inverted"].toBool();
            for (int i = 0; i < 3; ++i) t.bindings[i] = j[QString("keys%1").arg(i)].toArray();
            tasks.push_back(std::move(t));
        }
    }
    bool save()
    {
        if (!canSave) {
            saveStatus->setText("Settings could not be read. Existing file is protected; changes are temporary.");
            saveStatus->show(); return false;
        }
        captureHotkeys();
        QJsonArray rows;
        for (const auto &t : tasks) {
            QJsonObject o{{"id", t.id}, {"title", t.title}, {"rule", t.rule}, {"target", t.target},
                          {"detail", t.detail}, {"checked", t.rule == "custom" && t.checked}, {"inverted", t.inverted}};
            for (int i = 0; i < 3; ++i) o[QString("keys%1").arg(i)] = t.bindings[i];
            rows.append(o);
        }

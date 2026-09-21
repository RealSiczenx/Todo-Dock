        const QByteArray bytes = QJsonDocument(QJsonObject{{"version", 1}, {"tasks", rows}, {"accent", accent},
            {"fontSize", fontSize}, {"compact", compact}, {"showProgress", showProgress}}).toJson();
        QDir().mkpath(QFileInfo(configPath).absolutePath());
        QSaveFile f(configPath);
        const bool ok = f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size() && f.commit();
        saveStatus->setVisible(!ok);
        if (!ok) saveStatus->setText("Could not save tasks. Check folder permissions.");
        return ok;
    }
    void styleDock()
    {
        setStyleSheet(QString(
            "QWidget#taskDock { font-size:%1px; }"
            "QListWidget#taskList { border:1px solid palette(mid); border-radius:7px; padding:4px; }"
            "QListWidget#taskList::item { padding:%2px 6px; border-radius:4px; }"
            "QListWidget#taskList::item:selected { background:palette(highlight); color:palette(highlighted-text); }"
            "QToolButton { padding:3px 6px; }"
            "QProgressBar { border:0; background:palette(mid); border-radius:3px; max-height:5px; }"
            "QProgressBar::chunk { background:%3; border-radius:3px; }"
        ).arg(fontSize).arg(compact ? 5 : 11).arg(accent));
        QFont f = list->font(); f.setPixelSize(fontSize); list->setFont(f);
        progress->setVisible(showProgress);
    }
    void updateButtons()
    {
        const int row = list->currentRow();
        removeButton->setEnabled(row >= 0);
        upButton->setEnabled(row > 0);
        downButton->setEnabled(row >= 0 && row + 1 < int(tasks.size()));
    }
    void rebuild()
    {
        const int selected = list->currentRow();
        QSignalBlocker blocker(list);
        list->clear();
        for (const auto &t : tasks) {
            auto *item = new QListWidgetItem(list);
            item->setData(Qt::UserRole, t.id);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | (t.rule == "custom" ? Qt::ItemIsUserCheckable : Qt::NoItemFlags));
        }
        if (!tasks.empty()) list->setCurrentRow(std::clamp(selected, 0, int(tasks.size()) - 1));
        refresh(); updateButtons();
    }
    void moveTask(int delta)
    {
        const int from = list->currentRow(), to = taskdock::movedIndex(from, delta, int(tasks.size()));
        if (to < 0 || from == to) return;
        std::swap(tasks[from], tasks[to]); rebuild(); list->setCurrentRow(to); save();
    }
    QStringList choices(int kind)
    {
        QStringList result;
        if (kind == 2 || kind == 7 || kind == 5) {
            obs_frontend_source_list sources = {};
            if (kind == 5) obs_frontend_get_transitions(&sources); else obs_frontend_get_scenes(&sources);
            for (size_t i = 0; i < sources.sources.num; ++i) result << sourceName(sources.sources.array[i]);
            obs_frontend_source_list_free(&sources);
        } else if (kind == 3 || kind == 4) {
            char **names = kind == 3 ? obs_frontend_get_profiles() : obs_frontend_get_scene_collections();
            if (names) { for (char **n = names; *n; ++n) result << QString::fromUtf8(*n); bfree(names); }
        } else if (kind == 1 || kind == 6) {
            obs_enum_sources([](void *data, obs_source_t *s) {
                static_cast<QStringList *>(data)->append(sourceName(s)); return true;
            }, &result);
        }
        result.removeDuplicates(); result.sort(Qt::CaseInsensitive); return result;
    }
    void editTask(int index, bool automatic = false)
    {
        const bool editing = index >= 0 && index < int(tasks.size());
        Task draft = editing ? tasks[index] : Task{};
        if (!editing) draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QDialog dialog(this); dialog.setWindowTitle(editing ? "Edit task" : "Add task"); dialog.setMinimumWidth(410);
        auto *form = new QFormLayout(&dialog);
        auto *title = new QLineEdit(draft.title, &dialog); title->setMaxLength(160); title->setPlaceholderText("e.g. Check microphone before going live");
        auto *type = new QComboBox(&dialog); type->addItem("Custom task", "custom");
        for (const auto &r : rules) type->addItem(r.name, r.id);
        type->setCurrentIndex(editing ? std::max(0, type->findData(draft.rule)) : automatic ? 1 : 0);
        auto *target = new QComboBox(&dialog); target->setEditable(true);
        auto *detail = new QLineEdit(draft.detail, &dialog);
        auto *invert = new QCheckBox("Check when this condition is false", &dialog); invert->setChecked(draft.inverted);
        auto *note = new QLabel(&dialog); note->setWordWrap(true); note->setMinimumWidth(340);
        form->addRow("Task name", title); form->addRow("Task type", type);
        form->addRow("Target", target); form->addRow("Filter / item name", detail); form->addRow("", invert); form->addRow(note);
        auto update = [&] {
            const auto *r = findRule(type->currentData().toString());
            const int kind = r ? r->target : 0;
            const QString previous = target->currentText(); target->clear(); target->addItems(choices(kind));
            if (!previous.isEmpty()) target->setCurrentText(previous);
            target->setEnabled(kind != 0); detail->setEnabled(kind == 6 || kind == 7); invert->setEnabled(r != nullptr);
            note->setText(r ? "Automatic tasks follow OBS every 300 ms. Missing targets remain unchecked. Target names must match exactly."
                            : "Tick this task manually, or assign Toggle, Check and Uncheck shortcuts in OBS Settings → Hotkeys. Search for Task Dock after saving.");
        };
        connect(type, &QComboBox::currentIndexChanged, &dialog, update); update();
        if (!draft.target.isEmpty()) target->setCurrentText(draft.target);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog); form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
            const auto *r = findRule(type->currentData().toString());
            if (title->text().trimmed().isEmpty()) { title->setFocus(); return; }

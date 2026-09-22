        const QByteArray bytes = QJsonDocument(QJsonObject{
            {"version", 1},
            {"tasks", rows},
            {"accent", accent},
            {"fontSize", fontSize},
            {"compact", compact}
        }).toJson();

        QDir().mkpath(QFileInfo(configPath).absolutePath());
        QSaveFile f(configPath);
        const bool ok = f.open(QIODevice::WriteOnly) &&
                        f.write(bytes) == bytes.size() &&
                        f.commit();
        saveStatus->setVisible(!ok);
        if (!ok)
            saveStatus->setText("Could not save tasks. Check folder permissions.");
        return ok;
    }

    void styleDock()
    {
        // Deliberately do not apply a plugin stylesheet here. The dock is
        // hosted inside OBS's OBSDock, so the active OBS theme styles the
        // list, scrollbars, toolbar, separators and buttons exactly like the
        // built-in Scenes and Sources docks.
        setStyleSheet(QString());
        list->setFont(font());
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
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                           (t.rule == "custom" ? Qt::ItemIsUserCheckable : Qt::NoItemFlags));
        }

        if (!tasks.empty())
            list->setCurrentRow(std::clamp(selected, 0, int(tasks.size()) - 1));
        refresh();
        updateButtons();
    }

    void moveTask(int delta)
    {
        const int from = list->currentRow();
        const int to = taskdock::movedIndex(from, delta, int(tasks.size()));
        if (to < 0 || from == to) return;

        std::swap(tasks[from], tasks[to]);
        rebuild();
        list->setCurrentRow(to);
        save();
    }

    QStringList choices(int kind)
    {
        QStringList result;

        if (kind == 2 || kind == 7 || kind == 5) {
            obs_frontend_source_list sources = {};
            if (kind == 5)
                obs_frontend_get_transitions(&sources);
            else
                obs_frontend_get_scenes(&sources);

            for (size_t i = 0; i < sources.sources.num; ++i)
                result << sourceName(sources.sources.array[i]);
            obs_frontend_source_list_free(&sources);
        } else if (kind == 3 || kind == 4) {
            char **names = kind == 3 ? obs_frontend_get_profiles() : obs_frontend_get_scene_collections();
            if (names) {
                for (char **n = names; *n; ++n)
                    result << QString::fromUtf8(*n);
                bfree(names);
            }
        } else if (kind == 1 || kind == 6) {
            obs_enum_sources([](void *listContext, obs_source_t *s) {
                static_cast<QStringList *>(listContext)->append(sourceName(s));
                return true;
            }, &result);
        } else if (kind == 8) {
            obs_enum_canvases([](void *listContext, obs_canvas_t *canvas) {
                const char *name = obs_canvas_get_name(canvas);
                if (name && *name)
                    static_cast<QStringList *>(listContext)->append(QString::fromUtf8(name));
                return true;
            }, &result);
        }

        result.removeDuplicates();
        result.sort(Qt::CaseInsensitive);
        return result;
    }

    QStringList detailChoices(int kind, const QString &targetName)
    {
        QStringList result;
        if (targetName.isEmpty()) return result;

        obs_source_t *source = obs_get_source_by_name(targetName.toUtf8().constData());
        if (!source) return result;

        if (kind == 6) {
            obs_source_enum_filters(source, [](obs_source_t *, obs_source_t *filter, void *listContext) {
                static_cast<QStringList *>(listContext)->append(sourceName(filter));
            }, &result);
        } else if (kind == 7) {
            obs_scene_t *scene = obs_scene_from_source(source);
            if (scene) {
                obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *listContext) {
                    obs_source_t *itemSource = obs_sceneitem_get_source(item);
                    if (itemSource)
                        static_cast<QStringList *>(listContext)->append(sourceName(itemSource));
                    return true;
                }, &result);
            }
        }

        obs_source_release(source);
        result.removeDuplicates();
        result.sort(Qt::CaseInsensitive);
        return result;
    }

    void editTask(int index, bool automatic = false)
    {
        const bool editing = index >= 0 && index < int(tasks.size());
        Task draft = editing ? tasks[index] : Task{};
        if (!editing)
            draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

        QDialog dialog(this);
        dialog.setWindowTitle(editing ? "Edit task" : "Add task");
        dialog.setMinimumWidth(450);

        auto *form = new QFormLayout(&dialog);
        auto *title = new QLineEdit(draft.title, &dialog);
        title->setMaxLength(160);
        title->setPlaceholderText("e.g. Check microphone before going live");

        auto *type = new QComboBox(&dialog);
        type->addItem("Custom task", "custom");
        for (const auto &r : rules)
            type->addItem(r.name, r.id);
        type->setMaxVisibleItems(24);
        type->setCurrentIndex(editing ? std::max(0, type->findData(draft.rule)) : automatic ? 1 : 0);

        auto *target = new QComboBox(&dialog);
        target->setEditable(true);
        auto *detail = new QComboBox(&dialog);
        detail->setEditable(true);
        detail->setCurrentText(draft.detail);

        auto *invert = new QCheckBox("Check when this condition is false", &dialog);
        invert->setChecked(draft.inverted);
        auto *note = new QLabel(&dialog);
        note->setWordWrap(true);
        note->setMinimumWidth(360);

        form->addRow("Task name", title);
        form->addRow("Task type", type);
        form->addRow("Target", target);
        form->addRow("Filter / scene item", detail);
        form->addRow("", invert);
        form->addRow(note);

        auto updateDetail = [&] {
            const auto *r = findRule(type->currentData().toString());
            const int kind = r ? r->target : 0;
            const QString previous = detail->currentText();
            detail->clear();
            detail->addItems(detailChoices(kind, target->currentText()));
            if (!previous.isEmpty())
                detail->setCurrentText(previous);
            detail->setEnabled(kind == 6 || kind == 7);
        };

        auto update = [&] {
            const auto *r = findRule(type->currentData().toString());
            const int kind = r ? r->target : 0;
            const QString previous = target->currentText();

            target->clear();
            target->addItems(choices(kind));
            if (!previous.isEmpty())
                target->setCurrentText(previous);

            target->setEnabled(kind != 0);
            invert->setEnabled(r != nullptr);

            if (!editing && automatic && r && title->text().trimmed().isEmpty()) {
                QString suggested = QString::fromUtf8(r->name);
                const int separator = suggested.indexOf(" • ");
                if (separator >= 0) suggested = suggested.mid(separator + 3);
                title->setText(suggested);
            }

            updateDetail();
            note->setText(r
                ? "Automatic tasks follow native OBS state every 300 ms. Source, scene, filter, item and canvas targets can be selected from the current OBS configuration. Invert any state with the checkbox."
                : "Tick this task manually, or assign Toggle, Check and Uncheck shortcuts in OBS Settings → Hotkeys. Search for Task Dock after saving.");
        };

        connect(type, &QComboBox::currentIndexChanged, &dialog, update);
        connect(target, &QComboBox::currentTextChanged, &dialog, updateDetail);
        update();

        if (!draft.target.isEmpty())
            target->setCurrentText(draft.target);
        if (!draft.detail.isEmpty())
            detail->setCurrentText(draft.detail);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
        form->addRow(buttons);

        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
            const auto *r = findRule(type->currentData().toString());
            if (title->text().trimmed().isEmpty()) {
                title->setFocus();
                return;
            }

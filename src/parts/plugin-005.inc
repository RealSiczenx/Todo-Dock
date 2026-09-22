            if (r && r->target && target->currentText().trimmed().isEmpty()) {
                target->setFocus();
                return;
            }
            if (r && (r->target == 6 || r->target == 7) && detail->currentText().trimmed().isEmpty()) {
                detail->setFocus();
                return;
            }
            dialog.accept();
        });

        if (dialog.exec() != QDialog::Accepted) return;

        captureHotkeys();
        if (editing)
            for (int i = 0; i < 3; ++i)
                draft.bindings[i] = tasks[index].bindings[i];

        draft.title = title->text().trimmed();
        draft.rule = type->currentData().toString();
        draft.target = target->currentText().trimmed();
        draft.detail = detail->currentText().trimmed();
        draft.inverted = invert->isChecked();

        if (editing && tasks[index].rule == "custom" && draft.rule == "custom")
            draft.checked = tasks[index].checked;
        if (editing && tasks[index].rule != draft.rule)
            draft.checked = false;

        unregisterHotkeys();
        if (editing)
            tasks[index] = draft;
        else
            tasks.push_back(draft);

        for (auto &t : tasks)
            registerTask(t);

        rebuild();
        list->setCurrentRow(editing ? index : int(tasks.size()) - 1);
        save();
    }

public:
    explicit TaskDock(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName("taskDock");
        setMinimumSize(150, 110);
        resize(260, 320);

        // Match the built-in latest OBS Scenes/Sources dock hierarchy:
        // 1 px outer edge, zero spacing, frameless item view, native QToolBar.
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(1, 0, 1, 1);
        layout->setSpacing(0);

        auto *frame = new QFrame(this);
        auto *frameLayout = new QVBoxLayout(frame);
        frameLayout->setContentsMargins(0, 0, 0, 0);
        frameLayout->setSpacing(0);

        list = new QListWidget(frame);
        list->setObjectName("taskList");
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setAccessibleName("Tasks");
        list->setContextMenuPolicy(Qt::CustomContextMenu);
        list->setFrameShape(QFrame::NoFrame);
        list->setFrameShadow(QFrame::Plain);
        list->setSpacing(0);
        list->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

        saveStatus = new QLabel(frame);
        saveStatus->setWordWrap(true);
        saveStatus->hide();

        auto *toolbar = new QToolBar(frame);
        toolbar->setIconSize(QSize(16, 16));
        toolbar->setFloatable(false);
        toolbar->setMovable(false);
        toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

        auto *add = makeAction("Add", "Add task", "icon-plus", toolbar);
        removeButton = makeAction("Remove", "Delete selected task", "icon-trash", toolbar);
        auto *settings = makeAction("Properties", "Task Dock options", "icon-gear", toolbar);
        upButton = makeAction("Move Up", "Move selected task up", "icon-up", toolbar);
        downButton = makeAction("Move Down", "Move selected task down", "icon-down", toolbar);

        removeButton->setShortcut(QKeySequence::Delete);
        removeButton->setShortcutContext(Qt::WidgetWithChildrenShortcut);

        toolbar->addAction(add);
        toolbar->addAction(removeButton);
        toolbar->addSeparator();
        toolbar->addAction(settings);
        toolbar->addSeparator();
        toolbar->addAction(upButton);
        toolbar->addAction(downButton);

        // Match the exact theme-driven icon behavior used by OBS itself.
        applyObsIconClass(toolbar, add, "icon-plus");
        applyObsIconClass(toolbar, removeButton, "icon-trash");
        applyObsIconClass(toolbar, settings, "icon-gear");
        applyObsIconClass(toolbar, upButton, "icon-up");
        applyObsIconClass(toolbar, downButton, "icon-down");

        frameLayout->addWidget(list, 1);
        frameLayout->addWidget(saveStatus);
        frameLayout->addWidget(toolbar);
        layout->addWidget(frame, 1);

        auto *addMenu = new QMenu(this);
        addMenu->addAction("Custom task…", this, [this] { editTask(-1); });
        addMenu->addAction("Automatic OBS task…", this, [this] { editTask(-1, true); });
        connect(add, &QAction::triggered, this, [toolbar, add, addMenu] {
            if (auto *button = toolbar->widgetForAction(add))
                addMenu->popup(button->mapToGlobal(QPoint(0, button->height())));
        });

        auto *menu = new QMenu(this);
        menu->addAction("Edit selected task…", this, [this] {
            if (list->currentRow() >= 0)
                editTask(list->currentRow());
        });
        menu->addAction("Hotkeys…", this, [this] {
            QMessageBox::information(
                this,
                "Task hotkeys",
                "Open OBS Settings → Hotkeys and search for Task Dock.\n\n"
                "Every custom task has Toggle, Check and Uncheck actions. "
                "Automatic tasks follow OBS state and cannot be manually overridden.");
        });
        menu->addSeparator();
        menu->addAction("Uncheck all custom tasks", this, [this] {
            if (QMessageBox::question(this, "Reset checklist", "Uncheck all custom tasks?") != QMessageBox::Yes)
                return;
            for (auto &t : tasks)
                if (t.rule == "custom")
                    t.checked = false;
            refresh();
            save();
        });
        connect(settings, &QAction::triggered, this, [toolbar, settings, menu] {
            if (auto *button = toolbar->widgetForAction(settings))
                menu->popup(button->mapToGlobal(QPoint(0, button->height())));
        });

        connect(removeButton, &QAction::triggered, this, [this] {
            const int row = list->currentRow();
            if (row < 0) return;
            if (QMessageBox::question(this, "Delete task", "Delete “" + tasks[row].title + "”?") != QMessageBox::Yes)
                return;

            captureHotkeys();
            unregisterHotkeys();
            tasks.erase(tasks.begin() + row);
            for (auto &t : tasks)
                registerTask(t);
            rebuild();
            save();
        });

        connect(upButton, &QAction::triggered, this, [this] { moveTask(-1); });
        connect(downButton, &QAction::triggered, this, [this] { moveTask(1); });
        connect(list, &QListWidget::currentRowChanged, this, [this] { updateButtons(); });

        // First click selects a task; clicking that same task again clears
        // selection. Clicking another task moves the selection to that task.
        connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
            const QString id = item->data(Qt::UserRole).toString();
            if (!clickedSelectionId.isEmpty() && clickedSelectionId == id) {
                list->clearSelection();
                list->setCurrentRow(-1);
                clickedSelectionId.clear();
            } else {
                clickedSelectionId = id;
                list->setCurrentItem(item);
                item->setSelected(true);
            }
            updateButtons();
        });

        connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
            editTask(list->row(item));
        });

        connect(list, &QListWidget::customContextMenuRequested, this, [this](QPoint pos) {
            if (auto *item = list->itemAt(pos)) {
                list->setCurrentItem(item);
                editTask(list->row(item));
            }
        });

        connect(list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
            if (auto *t = byId(item->data(Qt::UserRole).toString()); t && t->rule == "custom") {
                t->checked = item->checkState() == Qt::Checked;
                refresh();
                save();
            }
        });

        load();
        for (auto &t : tasks)
            registerTask(t);
        styleDock();
        rebuild();

        if (!canSave) {
            saveStatus->setText("Settings could not be loaded. Existing file is protected; changes are temporary.");
            saveStatus->show();
        }

        connect(&timer, &QTimer::timeout, this, [this] { refresh(); });
        timer.start(300);
    }

    ~TaskDock() override { shutdown(); }

    void refresh()
    {
        if (stopped || exiting) return;

        QSignalBlocker blocker(list);
        for (int i = 0; i < int(tasks.size()); ++i) {
            auto &t = tasks[i];
            auto *item = list->item(i);
            if (!item) continue;

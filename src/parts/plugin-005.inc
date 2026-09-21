            if (r && r->target && target->currentText().trimmed().isEmpty()) { target->setFocus(); return; }
            if (r && (r->target == 6 || r->target == 7) && detail->text().trimmed().isEmpty()) { detail->setFocus(); return; }
            dialog.accept();
        });
        if (dialog.exec() != QDialog::Accepted) return;
        captureHotkeys();
        if (editing) for (int i = 0; i < 3; ++i) draft.bindings[i] = tasks[index].bindings[i];
        draft.title = title->text().trimmed(); draft.rule = type->currentData().toString();
        draft.target = target->currentText().trimmed(); draft.detail = detail->text().trimmed(); draft.inverted = invert->isChecked();
        if (editing && tasks[index].rule == "custom" && draft.rule == "custom")
            draft.checked = tasks[index].checked;
        // Do not copy a live automatic check into a newly manual task.
        if (editing && tasks[index].rule != draft.rule) draft.checked = false;
        unregisterHotkeys();
        if (editing) tasks[index] = draft; else tasks.push_back(draft);
        for (auto &t : tasks) registerTask(t);
        rebuild(); list->setCurrentRow(editing ? index : int(tasks.size()) - 1); save();
    }
    void customize()
    {
        QDialog dialog(this); dialog.setWindowTitle("Customize Task Dock");
        auto *form = new QFormLayout(&dialog);
        auto *size = new QSpinBox(&dialog); size->setRange(10, 24); size->setValue(fontSize); size->setSuffix(" px");
        auto *density = new QCheckBox("Compact task rows", &dialog); density->setChecked(compact);
        auto *bar = new QCheckBox("Show completion bar", &dialog); bar->setChecked(showProgress);
        auto *color = new QPushButton("Choose accent color…", &dialog); QString selected = accent;
        connect(color, &QPushButton::clicked, &dialog, [&] {
            const auto c = QColorDialog::getColor(QColor(selected), &dialog, "Accent color");
            if (c.isValid()) selected = c.name();
        });
        form->addRow("Text size", size); form->addRow("", density); form->addRow("", bar); form->addRow("Accent", color);
        auto *note = new QLabel("The dock follows your OBS theme. Drag its title bar to move or float it.", &dialog); note->setWordWrap(true); form->addRow(note);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog); form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() == QDialog::Accepted) {
            fontSize = size->value(); compact = density->isChecked(); showProgress = bar->isChecked(); accent = selected;
            styleDock(); save();
        }
    }
public:
    explicit TaskDock(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName("taskDock"); setMinimumSize(260, 180); resize(340, 430);
        auto *layout = new QVBoxLayout(this); layout->setContentsMargins(12, 12, 12, 10); layout->setSpacing(10);
        auto *heading = new QLabel("TASK CHECKLIST", this); QFont headingFont = heading->font(); headingFont.setBold(true); heading->setFont(headingFont);
        subtitle = new QLabel(this);
        progress = new QProgressBar(this); progress->setTextVisible(false); progress->setRange(0, 100);
        list = new QListWidget(this); list->setObjectName("taskList"); list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setAccessibleName("Tasks"); list->setContextMenuPolicy(Qt::CustomContextMenu);
        empty = new QLabel("Ready when you are.\nAdd your first task with the + button.", this); empty->setAlignment(Qt::AlignCenter); empty->setWordWrap(true);
        saveStatus = new QLabel(this); saveStatus->setWordWrap(true); saveStatus->hide();
        auto *toolbar = new QHBoxLayout;
        auto *add = button("+", "Add task", this); removeButton = button("−", "Delete selected task", this);
        upButton = button("↑", "Move selected task up", this); downButton = button("↓", "Move selected task down", this);
        auto *settings = button("⚙", "Customize dock and task options", this);
        toolbar->addWidget(add); toolbar->addWidget(removeButton); toolbar->addSpacing(6); toolbar->addWidget(upButton); toolbar->addWidget(downButton); toolbar->addStretch(); toolbar->addWidget(settings);
        layout->addWidget(heading); layout->addWidget(subtitle); layout->addWidget(progress); layout->addWidget(empty); layout->addWidget(list, 1); layout->addLayout(toolbar); layout->addWidget(saveStatus);
        auto *addMenu = new QMenu(add); addMenu->addAction("Custom task…", this, [this] { editTask(-1); });
        addMenu->addAction("Automatic OBS task…", this, [this] { editTask(-1, true); }); add->setMenu(addMenu); add->setPopupMode(QToolButton::InstantPopup);
        auto *menu = new QMenu(settings); menu->addAction("Customize dock…", this, [this] { customize(); });
        menu->addAction("Edit selected task…", this, [this] { if (list->currentRow() >= 0) editTask(list->currentRow()); });
        menu->addAction("Hotkeys…", this, [this] {
            QMessageBox::information(this, "Task hotkeys", "Open OBS Settings → Hotkeys and search for Task Dock.\n\nEvery custom task has Toggle, Check and Uncheck actions. Assign keys and click Apply. Automatic tasks follow OBS state and cannot be manually overridden.");
        });
        menu->addSeparator(); menu->addAction("Uncheck all custom tasks", this, [this] {
            if (QMessageBox::question(this, "Reset checklist", "Uncheck all custom tasks?") != QMessageBox::Yes) return;
            for (auto &t : tasks) if (t.rule == "custom") t.checked = false;
            refresh(); save();
        }); settings->setMenu(menu); settings->setPopupMode(QToolButton::InstantPopup);
        connect(removeButton, &QToolButton::clicked, this, [this] {
            const int row = list->currentRow(); if (row < 0) return;
            if (QMessageBox::question(this, "Delete task", "Delete “" + tasks[row].title + "”?") != QMessageBox::Yes) return;
            captureHotkeys(); unregisterHotkeys(); tasks.erase(tasks.begin() + row);
            for (auto &t : tasks) registerTask(t);
            rebuild(); save();
        });
        connect(upButton, &QToolButton::clicked, this, [this] { moveTask(-1); });
        connect(downButton, &QToolButton::clicked, this, [this] { moveTask(1); });
        connect(list, &QListWidget::currentRowChanged, this, [this] { updateButtons(); });
        connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) { editTask(list->row(item)); });
        connect(list, &QListWidget::customContextMenuRequested, this, [this](QPoint pos) {
            if (auto *item = list->itemAt(pos)) { list->setCurrentItem(item); editTask(list->row(item)); }
        });
        connect(list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
            if (auto *t = byId(item->data(Qt::UserRole).toString()); t && t->rule == "custom") {
                t->checked = item->checkState() == Qt::Checked; refresh(); save();
            }
        });
        load(); for (auto &t : tasks) registerTask(t); styleDock(); rebuild();
        if (!canSave) { saveStatus->setText("Settings could not be loaded. Existing file is protected; changes are temporary."); saveStatus->show(); }
        connect(&timer, &QTimer::timeout, this, [this] { refresh(); }); timer.start(300);
    }
    ~TaskDock() override { shutdown(); }
    void refresh()
    {
        if (stopped || exiting) return;
        QSignalBlocker blocker(list); int done = 0;
        for (int i = 0; i < int(tasks.size()); ++i) {
            auto &t = tasks[i]; auto *item = list->item(i); if (!item) continue;

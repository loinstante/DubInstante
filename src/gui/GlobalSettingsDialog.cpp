#include "GlobalSettingsDialog.h"
#include "../core/SettingsManager.h"
#include "Palette.h"

#include <QMediaDevices>
#include <QAudioDevice>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QFrame>
#include <QScrollArea>
#include <QStyle>
#include <QKeyEvent>

GlobalSettingsDialog::GlobalSettingsDialog(QWidget *parent, int initialTab)
    : QDialog(parent), m_activeButton(nullptr) {
    
    // Group definitions
    m_videoActions = {
        "video_play_pause",
        "video_frame_back",
        "video_frame_forward",
        "video_seek_back_5s",
        "video_seek_forward_5s",
        "video_go_start",
        "view_fullscreen"
    };

    m_recordActions = {
        "record_start",
        "record_stop",
        "take_split",
        "edit_undo",
        "edit_redo"
    };

    m_audioActions = {
        "audio_volume_up",
        "audio_volume_down",
        "audio_volume_mute"
    };

    m_projectActions = {
        "project_open",
        "video_open",
        "project_save",
        "project_save_as",
        "project_export"
    };

    setupUi();
    populateAudioDevices();
    loadSettings();

    // Switch to initial tab
    if (initialTab >= 0 && initialTab < m_tabButtons.size()) {
        m_tabButtons[initialTab]->click();
    }
}

void GlobalSettingsDialog::setupUi() {
    setObjectName("globalSettingsDialog");
    setWindowTitle(tr("Global Settings"));
    setMinimumSize(660, 520);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // Title & Subtitle
    QLabel *titleLabel = new QLabel(tr("Global Settings"), this);
    titleLabel->setObjectName("settingsDialogTitle");
    mainLayout->addWidget(titleLabel);

    QLabel *subtitleLabel = new QLabel(tr("Set your studio preferences (theme, autosave, audio routing)."), this);
    subtitleLabel->setObjectName("settingsDialogSubtitle");
    mainLayout->addWidget(subtitleLabel);

    // Sidebar Layout (Sidebar + Content Stack)
    QHBoxLayout *bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(18);

    // Sidebar
    QFrame *sidebar = new QFrame(this);
    sidebar->setObjectName("settingsSidebarCard");
    sidebar->setFixedWidth(160);
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(8, 8, 8, 8);
    sidebarLayout->setSpacing(6);

    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true);

    QStringList tabLabels = { tr("General"), tr("Audio & Microphones"), tr("Keyboard Shortcuts") };
    for (int i = 0; i < tabLabels.size(); ++i) {
        QPushButton *btn = new QPushButton(tabLabels[i], sidebar);
        btn->setCheckable(true);
        btn->setProperty("cssClass", "settingsTabButton");
        btn->setMinimumHeight(34);
        if (i == 0) btn->setChecked(true);
        m_tabGroup->addButton(btn, i);
        m_tabButtons.append(btn);
        sidebarLayout->addWidget(btn);
    }
    sidebarLayout->addStretch();
    bodyLayout->addWidget(sidebar);

    // Content Stack
    m_stackedWidget = new QStackedWidget(this);

    // ==========================================
    // TAB 1: General Settings
    // ==========================================
    QFrame *generalPage = new QFrame(m_stackedWidget);
    generalPage->setObjectName("settingsCard");
    QVBoxLayout *genLayout = new QVBoxLayout(generalPage);
    genLayout->setContentsMargins(18, 18, 18, 18);
    genLayout->setSpacing(14);

    QFormLayout *genForm = new QFormLayout();
    genForm->setVerticalSpacing(12);
    genForm->setHorizontalSpacing(10);

    // Theme Selector
    QLabel *themeLabel = new QLabel(tr("Visual theme"), generalPage);
    themeLabel->setProperty("cssClass", "fineLabel");
    m_themeCombo = new QComboBox(generalPage);
    m_themeCombo->addItem(tr("Automatic (system)"), "system");
    m_themeCombo->addItem(tr("Light Mode"), "light");
    m_themeCombo->addItem(tr("Premium Dark Mode"), "dark");
    genForm->addRow(themeLabel, m_themeCombo);

    // Language: every name in its own language, whatever the current one
    QLabel *languageLabel = new QLabel(tr("Language"), generalPage);
    languageLabel->setProperty("cssClass", "fineLabel");
    m_languageCombo = new QComboBox(generalPage);
    m_languageCombo->addItem(tr("System (%1)").arg(languageName(
        SettingsManager::shippedLanguage(QLocale::system()))), "system");
    for (const QString &code : SettingsManager::shippedLanguages())
        m_languageCombo->addItem(languageName(code), code);
    genForm->addRow(languageLabel, m_languageCombo);

    // Countdown Selector (Stepper layout)
    QLabel *countdownLabel = new QLabel(tr("Pre-recording countdown"), generalPage);
    countdownLabel->setProperty("cssClass", "fineLabel");

    m_countdown.zeroText = tr("Off (instant)");
    genForm->addRow(countdownLabel, createStepper(generalPage, m_countdown));

    // Pre-roll: playback starts earlier so the actor hears the lead-in
    //: Pre-roll: playback before the point where recording replaces the old take (punch-in)
    QLabel *preRollLabel = new QLabel(tr("Pre-roll before the punch-in"), generalPage);
    preRollLabel->setProperty("cssClass", "fineLabel");
    preRollLabel->setToolTip(tr("Playback starts this many seconds before the playhead; only what "
                                "follows the playhead replaces the previous take."));
    m_preRoll.zeroText = tr("Off");
    genForm->addRow(preRollLabel, createStepper(generalPage, m_preRoll));


    genLayout->addLayout(genForm);

    // Auto-save Group
    QGroupBox *autoSaveGroup = new QGroupBox(tr("Autosave (Cache)"), generalPage);
    QVBoxLayout *autoSaveLayout = new QVBoxLayout(autoSaveGroup);
    autoSaveLayout->setContentsMargins(14, 20, 14, 14);
    autoSaveLayout->setSpacing(10);

    m_autoSaveCheck = new QCheckBox(tr("Enable autosave to the cache"), autoSaveGroup);
    autoSaveLayout->addWidget(m_autoSaveCheck);

    QWidget *intervalWidget = new QWidget(autoSaveGroup);
    QHBoxLayout *intervalLayout = new QHBoxLayout(intervalWidget);
    intervalLayout->setContentsMargins(0, 0, 0, 0);
    intervalLayout->setSpacing(8);
    QLabel *intervalLabel = new QLabel(tr("Save frequency:"), intervalWidget);
    intervalLabel->setProperty("cssClass", "fineLabel");
    m_autoSaveIntervalCombo = new QComboBox(intervalWidget);
    for (int minutes : {1, 3, 5, 10, 15})
        m_autoSaveIntervalCombo->addItem(tr("Every %n minute(s)", nullptr, minutes), minutes);
    intervalLayout->addWidget(intervalLabel);
    intervalLayout->addWidget(m_autoSaveIntervalCombo);
    intervalLayout->addStretch();
    autoSaveLayout->addWidget(intervalWidget);

    connect(m_autoSaveCheck, &QCheckBox::toggled, intervalWidget, &QWidget::setEnabled);

    genLayout->addWidget(autoSaveGroup);
    genLayout->addStretch();
    m_stackedWidget->addWidget(generalPage);

    // ==========================================
    // TAB 2: Audio Settings
    // ==========================================
    QFrame *audioPage = new QFrame(m_stackedWidget);
    audioPage->setObjectName("settingsCard");
    QVBoxLayout *audLayout = new QVBoxLayout(audioPage);
    audLayout->setContentsMargins(18, 18, 18, 18);
    audLayout->setSpacing(14);

    QFormLayout *audForm = new QFormLayout();
    audForm->setVerticalSpacing(12);
    audForm->setHorizontalSpacing(10);

    // Default Microphone
    QLabel *micLabel = new QLabel(tr("Default microphone"), audioPage);
    micLabel->setProperty("cssClass", "fineLabel");
    m_defaultMicCombo = new QComboBox(audioPage);
    audForm->addRow(micLabel, m_defaultMicCombo);
    audLayout->addLayout(audForm);

    // Preferred Outputs Group
    QGroupBox *outputsGroup = new QGroupBox(tr("Quick Output Selection (Headphones/Monitors)"), audioPage);
    QVBoxLayout *outputsLayout = new QVBoxLayout(outputsGroup);
    outputsLayout->setContentsMargins(14, 20, 14, 14);
    outputsLayout->setSpacing(10);

    QHBoxLayout *listActionsLayout = new QHBoxLayout();
    m_outputsList = new QListWidget(outputsGroup);
    m_outputsList->setMinimumHeight(100);
    m_removeOutputBtn = new QPushButton(tr("Remove (−)"), outputsGroup);
    m_removeOutputBtn->setObjectName("settingsCancelButton");
    m_removeOutputBtn->setMinimumHeight(30);

    listActionsLayout->addWidget(m_outputsList, 1);
    listActionsLayout->addWidget(m_removeOutputBtn, 0, Qt::AlignTop);
    outputsLayout->addLayout(listActionsLayout);

    // Add Output Area
    QFrame *addFrame = new QFrame(outputsGroup);
    addFrame->setObjectName("settingsCardTrackSelector");
    QFormLayout *addForm = new QFormLayout(addFrame);
    addForm->setContentsMargins(10, 8, 10, 8);
    addForm->setSpacing(6);

    m_newOutputNameEdit = new QLineEdit(addFrame);
    m_newOutputNameEdit->setPlaceholderText(tr("e.g. My Sony Headphones, Studio Speakers..."));
    m_newOutputDeviceCombo = new QComboBox(addFrame);

    m_addOutputBtn = new QPushButton(tr("Add to favorites (+)"), addFrame);
    m_addOutputBtn->setProperty("cssClass", "presetButton");
    m_addOutputBtn->setMinimumHeight(30);

    addForm->addRow(tr("Custom name:"), m_newOutputNameEdit);
    addForm->addRow(tr("Device:"), m_newOutputDeviceCombo);
    addForm->addRow(m_addOutputBtn);

    outputsLayout->addWidget(addFrame);
    audLayout->addWidget(outputsGroup);
    audLayout->addStretch();
    m_stackedWidget->addWidget(audioPage);

    // ==========================================
    // TAB 3: Shortcuts Settings
    // ==========================================
    QFrame *shortcutsPage = new QFrame(m_stackedWidget);
    shortcutsPage->setObjectName("settingsCard");
    QVBoxLayout *shortcutsLayout = new QVBoxLayout(shortcutsPage);
    shortcutsLayout->setContentsMargins(18, 18, 18, 18);
    shortcutsLayout->setSpacing(12);

    // Scroll Area for Shortcuts
    QScrollArea *scrollArea = new QScrollArea(shortcutsPage);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background: transparent;");
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *scrollContent = new QWidget(scrollArea);
    scrollContent->setStyleSheet("background: transparent;");
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(14);

    // Helper function to create categories in the settings panel
    auto createCategoryGroup = [this, shortcutsPage](const QString &title, const QStringList &actions, QVBoxLayout *parentLayout) {
        QGroupBox *group = new QGroupBox(title, shortcutsPage);
        QGridLayout *gridLayout = new QGridLayout(group);
        gridLayout->setContentsMargins(14, 20, 14, 14);
        gridLayout->setHorizontalSpacing(12);
        gridLayout->setVerticalSpacing(10);

        int row = 0;
        for (const QString &actionId : actions) {
            // Label
            QLabel *label = new QLabel(getActionName(actionId), group);
            label->setProperty("cssClass", "fineLabel");
            gridLayout->addWidget(label, row, 0);

            // Shortcut button
            QPushButton *shortcutBtn = new QPushButton(group);
            shortcutBtn->setProperty("cssClass", "presetButton");
            shortcutBtn->setMinimumHeight(30);
            shortcutBtn->setMinimumWidth(150);
            shortcutBtn->setCursor(Qt::PointingHandCursor);
            connect(shortcutBtn, &QPushButton::clicked, this, [this, actionId]() {
                onShortcutButtonClicked(actionId);
            });
            m_shortcutButtons[actionId] = shortcutBtn;
            gridLayout->addWidget(shortcutBtn, row, 1);

            // Clear button
            QPushButton *clearBtn = new QPushButton("×", group);
            clearBtn->setProperty("cssClass", "shortcutClearButton");
            clearBtn->setFixedSize(30, 30);
            clearBtn->setCursor(Qt::PointingHandCursor);
            clearBtn->setToolTip(tr("Clear the shortcut"));
            connect(clearBtn, &QPushButton::clicked, this, [this, actionId]() {
                onClearShortcut(actionId);
            });
            m_clearButtons[actionId] = clearBtn;
            gridLayout->addWidget(clearBtn, row, 2);

            row++;
        }
        gridLayout->setColumnStretch(0, 1); // Expand the label column
        parentLayout->addWidget(group);
    };

    createCategoryGroup(tr("Video Controls"), m_videoActions, scrollLayout);
    createCategoryGroup(tr("Recording"), m_recordActions, scrollLayout);
    createCategoryGroup(tr("Audio Controls"), m_audioActions, scrollLayout);
    createCategoryGroup(tr("Project"), m_projectActions, scrollLayout);

    scrollArea->setWidget(scrollContent);
    shortcutsLayout->addWidget(scrollArea, 1);

    // Reset Defaults Row in shortcuts tab
    QHBoxLayout *resetRow = new QHBoxLayout();
    QPushButton *resetBtn = new QPushButton(tr("Restore default shortcuts"), shortcutsPage);
    resetBtn->setObjectName("settingsCancelButton");
    resetBtn->setMinimumHeight(32);
    resetBtn->setMinimumWidth(200);
    resetBtn->setCursor(Qt::PointingHandCursor);
    connect(resetBtn, &QPushButton::clicked, this, &GlobalSettingsDialog::onResetShortcutsToDefaults);
    resetRow->addWidget(resetBtn);
    resetRow->addStretch();
    shortcutsLayout->addLayout(resetRow);

    m_stackedWidget->addWidget(shortcutsPage);

    bodyLayout->addWidget(m_stackedWidget, 1);
    mainLayout->addLayout(bodyLayout);

    // Bottom Action Buttons
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();

    QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
    cancelBtn->setObjectName("settingsCancelButton");
    cancelBtn->setMinimumHeight(34);
    cancelBtn->setMinimumWidth(100);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    QPushButton *saveBtn = new QPushButton(tr("Save"), this);
    saveBtn->setObjectName("settingsSaveButton");
    saveBtn->setMinimumHeight(34);
    saveBtn->setMinimumWidth(120);
    connect(saveBtn, &QPushButton::clicked, this, &GlobalSettingsDialog::saveSettings);

    bottomLayout->addWidget(cancelBtn);
    bottomLayout->addWidget(saveBtn);
    mainLayout->addLayout(bottomLayout);

    // Connections
    connect(m_tabGroup, &QButtonGroup::idClicked, this, &GlobalSettingsDialog::onTabChanged);
    connect(m_addOutputBtn, &QPushButton::clicked, this, &GlobalSettingsDialog::addPreferredOutput);
    connect(m_removeOutputBtn, &QPushButton::clicked, this, &GlobalSettingsDialog::removePreferredOutput);
}

void GlobalSettingsDialog::onTabChanged(int index) {
    m_stackedWidget->setCurrentIndex(index);
}

void GlobalSettingsDialog::populateAudioDevices() {
    m_inputDevices = QMediaDevices::audioInputs();
    m_outputDevices = QMediaDevices::audioOutputs();

    m_defaultMicCombo->clear();
    m_defaultMicCombo->addItem(tr("Use the system default microphone"), "");
    for (const QAudioDevice &device : m_inputDevices) {
        m_defaultMicCombo->addItem(device.description(), device.description());
    }

    m_newOutputDeviceCombo->clear();
    for (const QAudioDevice &device : m_outputDevices) {
        m_newOutputDeviceCombo->addItem(device.description(), device.description());
    }
}

void GlobalSettingsDialog::loadSettings() {
    SettingsManager &sm = SettingsManager::instance();

    // General tab
    int themeIdx = m_themeCombo->findData(sm.theme());
    if (themeIdx >= 0) m_themeCombo->setCurrentIndex(themeIdx);
    const int languageIdx = m_languageCombo->findData(sm.language());
    m_languageCombo->setCurrentIndex(qMax(0, languageIdx));

    m_countdown.seconds = sm.countdownDuration();
    updateStepper(m_countdown);
    m_preRoll.seconds = sm.preRollSeconds();
    updateStepper(m_preRoll);

    m_autoSaveCheck->setChecked(sm.autoSaveEnabled());
    int intervalIdx = m_autoSaveIntervalCombo->findData(sm.autoSaveInterval());
    if (intervalIdx >= 0) m_autoSaveIntervalCombo->setCurrentIndex(intervalIdx);

    // Audio tab
    int micIdx = m_defaultMicCombo->findData(sm.defaultMicrophone());
    if (micIdx >= 0) m_defaultMicCombo->setCurrentIndex(micIdx);

    m_outputsList->clear();
    int count = 1;
    for (const QString &out : sm.preferredOutputs()) {
        QStringList parts = out.split("|");
        if (parts.size() >= 2) {
            auto *item = new QListWidgetItem(QString("%1 -- %2 (%3)").arg(QString::number(count), parts[0], parts[1]));
            item->setData(Qt::UserRole, QString("%1|%2").arg(parts[0], parts[1]));
            m_outputsList->addItem(item);
            count++;
        }
    }

    // Load shortcuts
    QStringList allActions = m_videoActions + m_recordActions + m_audioActions + m_projectActions;
    for (const QString &actionId : allActions) {
        m_tempShortcuts[actionId] = sm.shortcut(actionId);
    }
    updateShortcutButtons();
}

void GlobalSettingsDialog::addPreferredOutput() {
    QString label = m_newOutputNameEdit->text().trimmed();
    QString devDesc = m_newOutputDeviceCombo->currentData().toString();

    if (label.isEmpty()) {
        QMessageBox::warning(this, tr("Missing fields"), tr("Please give this output a name (e.g. Sony Headphones)."));
        return;
    }

    // Add to list widget immediately
    int count = m_outputsList->count() + 1;
    auto *item = new QListWidgetItem(QString("%1 -- %2 (%3)").arg(QString::number(count), label, devDesc));
    item->setData(Qt::UserRole, QString("%1|%2").arg(label, devDesc));
    m_outputsList->addItem(item);

    // Clear add fields
    m_newOutputNameEdit->clear();
}

void GlobalSettingsDialog::removePreferredOutput() {
    QListWidgetItem *item = m_outputsList->currentItem();
    if (!item) return;

    delete item;

    // Recalculate numbers
    for (int i = 0; i < m_outputsList->count(); ++i) {
        QListWidgetItem *listItem = m_outputsList->item(i);
        QString currentText = listItem->text();
        // Remove old number prefix e.g. "2 -- "
        int prefixIdx = currentText.indexOf(" -- ");
        if (prefixIdx >= 0) {
            listItem->setText(QString("%1 -- %2").arg(QString::number(i + 1), currentText.mid(prefixIdx + 4)));
        }
    }
}

void GlobalSettingsDialog::saveSettings() {
    SettingsManager &sm = SettingsManager::instance();

    // General settings
    sm.setTheme(m_themeCombo->currentData().toString());
    const QString language = m_languageCombo->currentData().toString();
    if (language != sm.language()) {
        sm.setLanguage(language);
        // Every label is built once: a restart is how they all change
        QMessageBox::information(this, tr("Language"),
                                 tr("Restart DubInstante to apply the language."));
    }
    sm.setCountdownDuration(m_countdown.seconds);
    sm.setPreRollSeconds(m_preRoll.seconds);
    sm.setAutoSaveEnabled(m_autoSaveCheck->isChecked());
    sm.setAutoSaveInterval(m_autoSaveIntervalCombo->currentData().toInt());

    // Audio settings
    sm.setDefaultMicrophone(m_defaultMicCombo->currentData().toString());

    // Preferred outputs list ("label|device" carried in UserRole: display text
    // is not parseable once the device name itself contains parentheses)
    QStringList outputs;
    for (int i = 0; i < m_outputsList->count(); ++i) {
        QString entry = m_outputsList->item(i)->data(Qt::UserRole).toString();
        if (entry.contains('|')) {
            outputs.append(entry);
        }
    }
    sm.setPreferredOutputs(outputs);

    // Save shortcuts
    for (auto it = m_tempShortcuts.begin(); it != m_tempShortcuts.end(); ++it) {
        sm.setShortcut(it.key(), it.value());
    }

    accept();
}

void GlobalSettingsDialog::updateShortcutButtons() {
    for (auto it = m_tempShortcuts.begin(); it != m_tempShortcuts.end(); ++it) {
        QString actionId = it.key();
        QKeySequence seq = it.value();
        
        QPushButton *btn = m_shortcutButtons.value(actionId, nullptr);
        if (btn) {
            if (seq.isEmpty()) {
                btn->setText(tr("None"));
                btn->setStyleSheet(QStringLiteral("color: %1; font-style: italic;").arg(QLatin1String(Brand::TextMuted)));
            } else {
                btn->setText(seq.toString(QKeySequence::NativeText));
                btn->setStyleSheet(""); // reset to stylesheet default
            }
        }
    }
}

// Empty code: no shipped translation matches, the English source is shown
QString GlobalSettingsDialog::languageName(const QString &code) {
    if (code.isEmpty() || code == "en")
        return QStringLiteral("English");
    const QLocale locale(code);
    QString name = locale.nativeLanguageName();
    // pt_BR reads "Português (Brasil)"; zh_CN already names its variant
    if (code.contains('_') && name == QLocale(locale.language()).nativeLanguageName())
        name += " (" + locale.nativeTerritoryName() + ")";
    return name.left(1).toUpper() + name.mid(1);
}

QString GlobalSettingsDialog::getActionName(const QString &actionId) const {
    if (actionId == "video_play_pause") return tr("Play / Pause");
    if (actionId == "video_frame_back") return tr("Back one frame (Previous)");
    if (actionId == "video_frame_forward") return tr("Forward one frame (Next)");
    if (actionId == "video_seek_back_5s") return tr("Back 5 seconds");
    if (actionId == "video_seek_forward_5s") return tr("Forward 5 seconds");
    if (actionId == "record_start") return tr("Start recording");
    if (actionId == "record_stop") return tr("Stop recording");
    //: Comp: the assembly of the best parts of several takes; a cut splits it in two regions
    if (actionId == "take_split") return tr("Add a comp cut");
    if (actionId == "audio_volume_up") return tr("Volume up");
    if (actionId == "audio_volume_down") return tr("Volume down");
    if (actionId == "audio_volume_mute") return tr("Mute / Unmute");
    if (actionId == "project_save") return tr("Save the project");
    if (actionId == "project_save_as") return tr("Save as...");
    if (actionId == "project_open") return tr("Open a project");
    if (actionId == "video_open") return tr("Open a video");
    if (actionId == "project_export") return tr("Export the dub");
    if (actionId == "edit_undo") return tr("Undo");
    if (actionId == "edit_redo") return tr("Redo");
    if (actionId == "video_go_start") return tr("Go to start");
    if (actionId == "view_fullscreen") return tr("Fullscreen (window)");
    return actionId;
}

void GlobalSettingsDialog::onShortcutButtonClicked(const QString &actionId) {
    // Escape is a bindable key, so a click is how a capture gets cancelled
    const bool wasCapturingThis = (m_capturingActionId == actionId);
    if (!m_capturingActionId.isEmpty()) {
        stopCapture(false);
    }
    if (wasCapturingThis) {
        return;
    }
    startCapture(actionId);
}

void GlobalSettingsDialog::onClearShortcut(const QString &actionId) {
    if (m_capturingActionId == actionId) {
        stopCapture(false);
    }
    m_tempShortcuts[actionId] = QKeySequence();
    updateShortcutButtons();
}

void GlobalSettingsDialog::startCapture(const QString &actionId) {
    m_capturingActionId = actionId;
    m_activeButton = m_shortcutButtons.value(actionId, nullptr);
    
    if (m_activeButton) {
        m_activeButton->setText(tr("Press a key (click to cancel)"));
        m_activeButton->setProperty("capturing", true);
        m_activeButton->style()->unpolish(m_activeButton);
        m_activeButton->style()->polish(m_activeButton);
        m_activeButton->setFocus();
    }
    
    grabKeyboard();
}

void GlobalSettingsDialog::stopCapture(bool acceptInput, const QKeySequence &seq) {
    releaseKeyboard();
    
    if (acceptInput && !m_capturingActionId.isEmpty()) {
        m_tempShortcuts[m_capturingActionId] = seq;
    }
    
    if (m_activeButton) {
        m_activeButton->setProperty("capturing", false);
        m_activeButton->style()->unpolish(m_activeButton);
        m_activeButton->style()->polish(m_activeButton);
    }

    m_capturingActionId.clear();
    m_activeButton = nullptr;
    
    updateShortcutButtons();
}

bool GlobalSettingsDialog::checkConflict(const QKeySequence &seq, const QString &currentActionId) {
    if (seq.isEmpty()) return false;

    for (auto it = m_tempShortcuts.begin(); it != m_tempShortcuts.end(); ++it) {
        if (it.key() != currentActionId && it.value() == seq &&
            !SettingsManager::sharesKeyByDesign(it.key(), currentActionId)) {
            QString otherActionName = getActionName(it.key());
            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                tr("Shortcut conflict"),
                tr("The shortcut '%1' is already assigned to '%2'.\n\nReassign it to this action and free the other one?")
                    .arg(seq.toString(QKeySequence::NativeText), otherActionName),
                QMessageBox::Yes | QMessageBox::No
            );
            
            if (reply == QMessageBox::Yes) {
                m_tempShortcuts[it.key()] = QKeySequence();
                return false;
            } else {
                return true;
            }
        }
    }
    return false;
}

void GlobalSettingsDialog::keyPressEvent(QKeyEvent *event) {
    if (!m_capturingActionId.isEmpty()) {
        int key = event->key();
        
        if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta) {
            event->accept();
            return;
        }

        int keyCombo = key;
        Qt::KeyboardModifiers modifiers = event->modifiers();
        
        if (modifiers & Qt::ShiftModifier)   keyCombo |= Qt::SHIFT;
        if (modifiers & Qt::ControlModifier) keyCombo |= Qt::CTRL;
        if (modifiers & Qt::AltModifier)     keyCombo |= Qt::ALT;
        if (modifiers & Qt::MetaModifier)    keyCombo |= Qt::META;

        QKeySequence seq(keyCombo);
        
        if (!checkConflict(seq, m_capturingActionId)) {
            stopCapture(true, seq);
        } else {
            stopCapture(false);
        }
        
        event->accept();
        return;
    }
    
    QDialog::keyPressEvent(event);
}

void GlobalSettingsDialog::mousePressEvent(QMouseEvent *event) {
    if (!m_capturingActionId.isEmpty()) {
        stopCapture(false);
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void GlobalSettingsDialog::onResetShortcutsToDefaults() {
    SettingsManager &sm = SettingsManager::instance();
    QStringList allActions = m_videoActions + m_recordActions + m_audioActions + m_projectActions;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Restore defaults"),
        tr("Restore every shortcut to its default value?"),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        for (const QString &actionId : allActions) {
            m_tempShortcuts[actionId] = sm.defaultShortcut(actionId);
        }
        updateShortcutButtons();
    }
}

QWidget *GlobalSettingsDialog::createStepper(QWidget *parent, SecondsStepper &stepper) {
    QWidget *widget = new QWidget(parent);
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    const auto makeButton = [widget](const QString &text) {
        QPushButton *button = new QPushButton(text, widget);
        button->setProperty("cssClass", "stepButton");
        button->setFixedSize(28, 28);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };
    stepper.down = makeButton("−");
    stepper.value = new QLabel(widget);
    stepper.value->setObjectName("countdownValueLabel");
    stepper.value->setAlignment(Qt::AlignCenter);
    stepper.up = makeButton("+");

    layout->addWidget(stepper.down);
    layout->addWidget(stepper.value);
    layout->addWidget(stepper.up);
    layout->addStretch();

    connect(stepper.down, &QPushButton::clicked, this, [this, &stepper]() {
        stepper.seconds = qMax(0, stepper.seconds - 1);
        updateStepper(stepper);
    });
    connect(stepper.up, &QPushButton::clicked, this, [this, &stepper]() {
        stepper.seconds = qMin(stepper.max, stepper.seconds + 1);
        updateStepper(stepper);
    });
    return widget;
}

void GlobalSettingsDialog::updateStepper(SecondsStepper &stepper) {
    if (stepper.seconds == 0) {
        stepper.value->setText(stepper.zeroText);
    } else {
        stepper.value->setText(tr("%n second(s)", nullptr, stepper.seconds));
    }
    stepper.down->setEnabled(stepper.seconds > 0);
    stepper.up->setEnabled(stepper.seconds < stepper.max);
}


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
    setWindowTitle(tr("Paramètres Globaux"));
    setMinimumSize(660, 520);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // Title & Subtitle
    QLabel *titleLabel = new QLabel(tr("Paramètres Globaux"), this);
    titleLabel->setObjectName("settingsDialogTitle");
    mainLayout->addWidget(titleLabel);

    QLabel *subtitleLabel = new QLabel(tr("Configurez les préférences de votre studio (thème, sauvegarde automatique, routages audio)."), this);
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

    QStringList tabLabels = { tr("Général"), tr("Audio & Micros"), tr("Raccourcis Clavier") };
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
    QLabel *themeLabel = new QLabel(tr("Thème visuel"), generalPage);
    themeLabel->setProperty("cssClass", "fineLabel");
    m_themeCombo = new QComboBox(generalPage);
    m_themeCombo->addItem(tr("Automatique (Système)"), "system");
    m_themeCombo->addItem(tr("Mode Clair"), "light");
    m_themeCombo->addItem(tr("Mode Sombre Premium"), "dark");
    genForm->addRow(themeLabel, m_themeCombo);

    // Language: every name in its own language, whatever the current one
    QLabel *languageLabel = new QLabel(tr("Langue"), generalPage);
    languageLabel->setProperty("cssClass", "fineLabel");
    m_languageCombo = new QComboBox(generalPage);
    m_languageCombo->addItem(tr("Système (%1)").arg(languageName(
        SettingsManager::shippedLanguage(QLocale::system()))), "system");
    for (const QString &code : SettingsManager::shippedLanguages())
        m_languageCombo->addItem(languageName(code), code);
    genForm->addRow(languageLabel, m_languageCombo);

    // Countdown Selector (Stepper layout)
    QLabel *countdownLabel = new QLabel(tr("Décompte pré-enregistrement"), generalPage);
    countdownLabel->setProperty("cssClass", "fineLabel");

    m_countdown.zeroText = tr("Désactivé (Instantané)");
    genForm->addRow(countdownLabel, createStepper(generalPage, m_countdown));

    // Pre-roll: playback starts earlier so the actor hears the lead-in
    QLabel *preRollLabel = new QLabel(tr("Pré-roll avant le point d'entrée"), generalPage);
    preRollLabel->setProperty("cssClass", "fineLabel");
    preRollLabel->setToolTip(tr("La lecture démarre ces secondes avant la tête de lecture ; "
                                "seul ce qui suit la tête de lecture remplace l'ancienne prise."));
    m_preRoll.zeroText = tr("Désactivé");
    genForm->addRow(preRollLabel, createStepper(generalPage, m_preRoll));


    genLayout->addLayout(genForm);

    // Auto-save Group
    QGroupBox *autoSaveGroup = new QGroupBox(tr("Sauvegarde Automatique (Cache)"), generalPage);
    QVBoxLayout *autoSaveLayout = new QVBoxLayout(autoSaveGroup);
    autoSaveLayout->setContentsMargins(14, 20, 14, 14);
    autoSaveLayout->setSpacing(10);

    m_autoSaveCheck = new QCheckBox(tr("Activer la sauvegarde automatique en cache"), autoSaveGroup);
    autoSaveLayout->addWidget(m_autoSaveCheck);

    QWidget *intervalWidget = new QWidget(autoSaveGroup);
    QHBoxLayout *intervalLayout = new QHBoxLayout(intervalWidget);
    intervalLayout->setContentsMargins(0, 0, 0, 0);
    intervalLayout->setSpacing(8);
    QLabel *intervalLabel = new QLabel(tr("Fréquence de sauvegarde :"), intervalWidget);
    intervalLabel->setProperty("cssClass", "fineLabel");
    m_autoSaveIntervalCombo = new QComboBox(intervalWidget);
    m_autoSaveIntervalCombo->addItem(tr("Chaque 1 minute"), 1);
    m_autoSaveIntervalCombo->addItem(tr("Chaque 3 minutes"), 3);
    m_autoSaveIntervalCombo->addItem(tr("Chaque 5 minutes"), 5);
    m_autoSaveIntervalCombo->addItem(tr("Chaque 10 minutes"), 10);
    m_autoSaveIntervalCombo->addItem(tr("Chaque 15 minutes"), 15);
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
    QLabel *micLabel = new QLabel(tr("Microphone par défaut"), audioPage);
    micLabel->setProperty("cssClass", "fineLabel");
    m_defaultMicCombo = new QComboBox(audioPage);
    audForm->addRow(micLabel, m_defaultMicCombo);
    audLayout->addLayout(audForm);

    // Preferred Outputs Group
    QGroupBox *outputsGroup = new QGroupBox(tr("Sélection rapide des Sorties (Casques/Moniteurs)"), audioPage);
    QVBoxLayout *outputsLayout = new QVBoxLayout(outputsGroup);
    outputsLayout->setContentsMargins(14, 20, 14, 14);
    outputsLayout->setSpacing(10);

    QHBoxLayout *listActionsLayout = new QHBoxLayout();
    m_outputsList = new QListWidget(outputsGroup);
    m_outputsList->setMinimumHeight(100);
    m_removeOutputBtn = new QPushButton(tr("Retirer (−)"), outputsGroup);
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
    m_newOutputNameEdit->setPlaceholderText(tr("ex. Mon Casque Sony, Enceintes Studio..."));
    m_newOutputDeviceCombo = new QComboBox(addFrame);

    m_addOutputBtn = new QPushButton(tr("Ajouter aux favoris (+)"), addFrame);
    m_addOutputBtn->setProperty("cssClass", "presetButton");
    m_addOutputBtn->setMinimumHeight(30);

    addForm->addRow(tr("Nom personnalisé :"), m_newOutputNameEdit);
    addForm->addRow(tr("Périphérique :"), m_newOutputDeviceCombo);
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
            clearBtn->setToolTip(tr("Supprimer le raccourci"));
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

    createCategoryGroup(tr("Contrôles Vidéo"), m_videoActions, scrollLayout);
    createCategoryGroup(tr("Enregistrement"), m_recordActions, scrollLayout);
    createCategoryGroup(tr("Contrôles Audio"), m_audioActions, scrollLayout);
    createCategoryGroup(tr("Projet"), m_projectActions, scrollLayout);

    scrollArea->setWidget(scrollContent);
    shortcutsLayout->addWidget(scrollArea, 1);

    // Reset Defaults Row in shortcuts tab
    QHBoxLayout *resetRow = new QHBoxLayout();
    QPushButton *resetBtn = new QPushButton(tr("Rétablir raccourcis par défaut"), shortcutsPage);
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

    QPushButton *cancelBtn = new QPushButton(tr("Annuler"), this);
    cancelBtn->setObjectName("settingsCancelButton");
    cancelBtn->setMinimumHeight(34);
    cancelBtn->setMinimumWidth(100);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    QPushButton *saveBtn = new QPushButton(tr("Enregistrer"), this);
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
    m_defaultMicCombo->addItem(tr("Utiliser le micro système par défaut"), "");
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
        QMessageBox::warning(this, tr("Champs manquants"), tr("Veuillez donner un nom à cette sortie (ex. Casque Sony)."));
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
        QMessageBox::information(this, tr("Langue"),
                                 tr("Redémarrez DubInstante pour appliquer la langue."));
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
                btn->setText(tr("Aucun"));
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
    if (actionId == "video_play_pause") return tr("Lecture / Pause");
    if (actionId == "video_frame_back") return tr("Reculer d'une image (Précédent)");
    if (actionId == "video_frame_forward") return tr("Avancer d'une image (Suivant)");
    if (actionId == "video_seek_back_5s") return tr("Reculer de 5 secondes");
    if (actionId == "video_seek_forward_5s") return tr("Avancer de 5 secondes");
    if (actionId == "record_start") return tr("Démarrer l'enregistrement");
    if (actionId == "record_stop") return tr("Arrêter l'enregistrement");
    if (actionId == "take_split") return tr("Ajouter une coupe au montage");
    if (actionId == "audio_volume_up") return tr("Augmenter le volume");
    if (actionId == "audio_volume_down") return tr("Diminuer le volume");
    if (actionId == "audio_volume_mute") return tr("Couper / Activer le son (Mute)");
    if (actionId == "project_save") return tr("Enregistrer le projet");
    if (actionId == "project_save_as") return tr("Enregistrer sous...");
    if (actionId == "project_open") return tr("Ouvrir un projet");
    if (actionId == "video_open") return tr("Ouvrir une vidéo");
    if (actionId == "project_export") return tr("Exporter le doublage");
    if (actionId == "edit_undo") return tr("Annuler");
    if (actionId == "edit_redo") return tr("Rétablir");
    if (actionId == "video_go_start") return tr("Aller au début");
    if (actionId == "view_fullscreen") return tr("Plein écran (fenêtre)");
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
        m_activeButton->setText(tr("Appuyez sur une touche (clic pour annuler)"));
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
                tr("Conflit de raccourci"),
                tr("Le raccourci '%1' est déjà attribué à '%2'.\n\nVoulez-vous le réattribuer à cette action et libérer l'autre ?")
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
        tr("Rétablir par défaut"),
        tr("Voulez-vous rétablir tous les raccourcis à leurs valeurs par défaut ?"),
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
    } else if (stepper.seconds == 1) {
        stepper.value->setText(tr("1 seconde"));
    } else {
        stepper.value->setText(tr("%1 secondes").arg(stepper.seconds));
    }
    stepper.down->setEnabled(stepper.seconds > 0);
    stepper.up->setEnabled(stepper.seconds < stepper.max);
}


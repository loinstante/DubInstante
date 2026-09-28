#ifndef GLOBALSETTINGSDIALOG_H
#define GLOBALSETTINGSDIALOG_H

#include <QDialog>
#include <QButtonGroup>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QListWidget>
#include <QLineEdit>
#include <QList>
#include <QAudioDevice>
#include <QMap>
#include <QKeySequence>

/**
 * @class GlobalSettingsDialog
 * @brief Dialogue for persistent application configuration.
 */
class GlobalSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit GlobalSettingsDialog(QWidget *parent = nullptr, int initialTab = 0);
    ~GlobalSettingsDialog() override = default;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void onTabChanged(int index);
    void addPreferredOutput();
    void removePreferredOutput();
    void saveSettings();
    void onShortcutButtonClicked(const QString &actionId);
    void onClearShortcut(const QString &actionId);
    void onResetShortcutsToDefaults();

private:
    void setupUi();
    void loadSettings();
    void populateAudioDevices();
    void startCapture(const QString &actionId);
    void stopCapture(bool acceptInput, const QKeySequence &seq = QKeySequence());
    bool checkConflict(const QKeySequence &seq, const QString &currentActionId);
    QString getActionName(const QString &actionId) const;
    void updateShortcutButtons();
    // A "− value +" row counting seconds, 0 shown as zeroText
    struct SecondsStepper {
        QLabel *value = nullptr;
        QPushButton *down = nullptr;
        QPushButton *up = nullptr;
        int seconds = 0;
        int max = 10;
        QString zeroText;
    };
    QWidget *createStepper(QWidget *parent, SecondsStepper &stepper);
    void updateStepper(SecondsStepper &stepper);

    // Tab control
    QButtonGroup *m_tabGroup;
    QList<QPushButton*> m_tabButtons;
    QStackedWidget *m_stackedWidget;

    // General Controls
    QComboBox *m_themeCombo;
    SecondsStepper m_countdown;
    SecondsStepper m_preRoll;

    QCheckBox *m_autoSaveCheck;
    QComboBox *m_autoSaveIntervalCombo;

    // Audio Controls
    QComboBox *m_defaultMicCombo;
    QListWidget *m_outputsList;
    QLineEdit *m_newOutputNameEdit;
    QComboBox *m_newOutputDeviceCombo;
    QPushButton *m_addOutputBtn;
    QPushButton *m_removeOutputBtn;

    // Cached hardware devices
    QList<QAudioDevice> m_inputDevices;
    QList<QAudioDevice> m_outputDevices;

    // Shortcuts Controls
    QStringList m_videoActions;
    QStringList m_recordActions;
    QStringList m_audioActions;
    QStringList m_projectActions;
    QMap<QString, QPushButton*> m_shortcutButtons;
    QMap<QString, QPushButton*> m_clearButtons;
    QMap<QString, QKeySequence> m_tempShortcuts;
    QString m_capturingActionId;
    QPushButton *m_activeButton;
};

#endif // GLOBALSETTINGSDIALOG_H

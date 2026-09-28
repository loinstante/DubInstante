/**
 * @file main.cpp
 * @brief Application entry point for DubInstante.
 * 
 * DubInstante is a professional dubbing studio application for
 * recording voice-over synchronized with video playback.
 */

#include "MainWindow.h"
#include "SettingsManager.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QStyleFactory>
#include <QTranslator>

// The language the app is shown in decides the rest: Qt's own dialogs and the
// layout direction follow it, so an untranslated system language stays all
// English instead of English with Korean buttons.
static void installTranslations(QApplication &app)
{
    const QString choice = SettingsManager::instance().language();
    const QLocale wanted = choice == "system" ? QLocale::system() : QLocale(choice);
    const QString shown = SettingsManager::shippedLanguage(wanted);
    if (shown.isEmpty())
        return;

    auto *appTranslator = new QTranslator(&app);
    if (appTranslator->load("dubinstante_" + shown, ":/i18n"))
        app.installTranslator(appTranslator);

    const QLocale shownLocale(shown);
    // Keep the user's region (dates) when it speaks the shown language
    QLocale::setDefault(wanted.language() == shownLocale.language() ? wanted : shownLocale);
    app.setLayoutDirection(shownLocale.textDirection());

    // Qt's catalogs sit next to the app once deployed: Windows, macOS bundle, AppImage
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList qtDirs = {QLibraryInfo::path(QLibraryInfo::TranslationsPath),
                                appDir + "/translations",
                                appDir + "/../Resources/translations",
                                appDir + "/../translations"};
    auto *qtTranslator = new QTranslator(&app);
    for (const QString &dir : qtDirs) {
        if (qtTranslator->load(shownLocale, "qt", "_", dir)) {
            app.installTranslator(qtTranslator);
            return;
        }
    }
    delete qtTranslator;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setStyle(QStyleFactory::create("Fusion"));

    // Application metadata
    app.setApplicationName("DubInstante");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("DubInstante");

    installTranslations(app);
    
    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}

/**
 * @file main.cpp
 * @brief Main program entry point
 * Responsible for initialising all necessary components,
 * setting application properties, creating the main window,
 * and handing control over to Qt's event loop.
 *
 */

#include <QApplication>          // The "heart" of a Qt application
#include <QMessageBox>           // Message dialog
#include <QStyleFactory>         // Style factory
#include <QDir>                  // Directory operations
#include <QStandardPaths>        // Standard paths
#include <QSettings>             // Settings manager
#include <QSplashScreen>         // Splash screen
#include <QPixmap>               // Pixmap
#include <QTimer>                // Timer

#include "cad_ui/MainWindow.h"   // Main window


 // QRC resource initialisation function declaration - manually initialise resources in static libraries
extern int qInitResources_resources();

// OpenCASCADE initialisation headers
#include <Standard_Version.hxx>      // Version info
#include <Message.hxx>               // Messaging system
#include <Message_PrinterOStream.hxx> // Output stream printer

/**
 * Main function
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return Exit code: 0 indicates success, any other value indicates an error
 */
int main(int argc, char* argv[])
{
    // Create the Qt application object
    QApplication app(argc, argv);

    // Manually initialise QRC resources to ensure resources in static libraries are loaded correctly
    qInitResources_resources();

    // Set application properties
    app.setApplicationName("JLi CAD");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("JLiCAD");


    // Initialise the OpenCASCADE message system
    // This lets us know what OpenCASCADE is doing and what goes wrong
    Handle(Message_PrinterOStream) printer = new Message_PrinterOStream();
    Message::DefaultMessenger()->AddPrinter(printer);

    // Create splash screen
    QSplashScreen* splash = nullptr;


    // Create the main window
    cad_ui::MainWindow mainWindow;

    // Initialise the main window
    if (!mainWindow.Initialize()) {

        return -1;
    }

    // Load user settings
    QSettings settings;
    if (settings.contains("geometry")) {
        // Restore window size and position
        mainWindow.restoreGeometry(settings.value("geometry").toByteArray());
    }
    if (settings.contains("windowState")) {
        // Restore window state (maximised, dock panel positions, etc.)
        mainWindow.restoreState(settings.value("windowState").toByteArray());
    }

    // Apply theme
    QString theme = settings.value("theme", "light").toString();  // Default: light theme
    mainWindow.SetTheme(theme);

    // Show the main window
    mainWindow.show();

    // Close the splash screen if one exists
    if (splash) {
        splash->finish(&mainWindow);
        delete splash;
    }


    // Run the application
    int result = app.exec();

    // Save settings before exiting
    QSettings saveSettings;
    saveSettings.setValue("geometry", mainWindow.saveGeometry());      // Save window size and position
    saveSettings.setValue("windowState", mainWindow.saveState());      // Save window state


    // Return the exit code
    return result;
}
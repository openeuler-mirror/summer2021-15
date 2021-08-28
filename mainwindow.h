#ifndef MAINWINDOW_H
#define MAINWINDOW_H

////////////////////////////////////////////////////////////////////////////////
// MainWindow is the main application window


#include <QDialog>
#include <QApplication>
#include <QCommandLineParser>

#include "common.h"

namespace Ui {
    class MainWindow;
}

class ExternalProgressBarPrivate;
class ExternalProgressBar
{
protected:
    ExternalProgressBarPrivate* const d_ptr;

public:
    ExternalProgressBar(QWidget* mainWindow);
    ~ExternalProgressBar();

    // Initializes the external progress bar and sets its limits
    bool InitProgressBar(quint64 maxSteps);

    // Deinitializes the external progress bar and returns into the normal state
    bool DestroyProgressBar();

    // Updates the current progress bar position
    bool SetProgressValue(quint64 currentSteps);

    // Sets the progress bar to indicate pause
    bool ProgressSetPause();

    // Sets the progress bar to indicate an error
    bool ProgressSetError();

protected:
    // Maximum counter value for the progress bar
    quint64 m_MaxValue;
};

class MainApplication : public QApplication
{
public:
    MainApplication(int& argc, char** argv);
    // Returns the language id to be used by the application (specified by --lang, or system locale otherwise)
    QString getLocale();
    // Returns the start-up directory that will be shown by default in the Open File dialog
    QString getInitialDir();
    // Returns the fila path passed to the application as command-line parameter
    QString getInitialImage();

protected:
    QCommandLineParser m_Options;
};

class MainWindow : public QDialog
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

protected:
    // Image file currently selected by the user
    QString m_ImageFile;
    // Size of the image file (cached here to avoid excessive file system requests)
    quint64 m_ImageSize;
    // Remember the last opened directory to suggest it automatically on next Open
    QString m_LastOpenedDir;
    // Whether image is being written at the moment or not
    bool    m_IsWriting;
    // Flag indicating that flash disks enumerating is pending
    bool    m_EnumFlashDevicesWaiting;

    // Abstraction layer for projecting the progress bar into operating system (if supported)
    ExternalProgressBar m_ExtProgressBar;

    // Retrieves information about the selected file and displays it in the dialog
    void preprocessImageFile(const QString& newImageFile);
    // Starts writing data to the device
    void writeToDevice(bool zeroing);
    // Frees the GUI-specific allocated resources
    void cleanup();

    // Reimplemented event handlers for drag&drop support
    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);

    // Reimplemented event handlers for protecting dialog closing during operation
    void closeEvent(QCloseEvent* event);
    void keyPressEvent(QKeyEvent* event);

    // Reloads the list of USB flash disks
    void enumFlashDevices();

public slots:
    // Suggests to select image file using the Open File dialog
    void openImageFile();
    // Schedules reloading the list of USB flash disks to run when possible
    void scheduleEnumFlashDevices();
    // Starts writing the image
    void writeImageToDevice();
    // Clears the selected USB device
    void clearDevice();
    // Verifies the selected USB device
    void verifyDevice();

    // Updates GUI to the "writing" mode (progress bar shown, controls disabled)
    // Also sets the progress bar limits
    void showWritingProgress(int maxValue);
    // Updates GUI to the "idle" mode (progress bar hidden, controls enabled)
    void hideWritingProgress();
    // Increments the progress bar counter by the specified number
    void updateProgressBar(int increment);
    // Displays the message about successful completion and returns to the "idle" mode
    void showSuccessMessage(QString msg);
    // Displays the specified error message and returns to the "idle" mode
    void showErrorMessage(QString msg);
};

#endif // MAINWINDOW_H

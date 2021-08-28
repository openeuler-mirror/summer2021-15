////////////////////////////////////////////////////////////////////////////////
// Implementation of MainWindow


#include <QMessageBox>
#include <QFileDialog>
#include <QDropEvent>
#include <QMimeData>
#include <QLocale>
#include <QProcess>
#include <QCryptographicHash>
#include <QThread>
#include <QTimer>
#include <QDir>
#include <QDebug>
#include <QTextStream>
#include <QFile>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QWidget>
#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>

#include "common.h"
#include "mainwindow.h"
#include "ui_main.h"
#include "imagewriter.h"
#include "usbdevice.h"


class ExternalProgressBarPrivate
{
public:
    ExternalProgressBarPrivate();
    ~ExternalProgressBarPrivate();
};

ExternalProgressBarPrivate::ExternalProgressBarPrivate()
{
}

ExternalProgressBarPrivate::~ExternalProgressBarPrivate()
{
}


ExternalProgressBar::ExternalProgressBar(QWidget* mainWindow) :
    d_ptr(new ExternalProgressBarPrivate()),
    m_MaxValue(0)
{
    Q_UNUSED(mainWindow);
}

ExternalProgressBar::~ExternalProgressBar()
{
    DestroyProgressBar();
    delete d_ptr;
}

// Initializes the external progress bar and sets its limits
bool ExternalProgressBar::InitProgressBar(quint64 maxSteps)
{
    m_MaxValue = maxSteps;
    Q_UNUSED(maxSteps);
    return false;
}

// Deinitializes the external progress bar and returns into the normal state
bool ExternalProgressBar::DestroyProgressBar()
{
    return false;
}

// Updates the current progress bar position
bool ExternalProgressBar::SetProgressValue(quint64 currentSteps)
{
    Q_UNUSED(currentSteps);
    return false;
}

// Sets the progress bar to indicate pause
bool ExternalProgressBar::ProgressSetPause()
{
    return false;
}

// Sets the progress bar to indicate an error
bool ExternalProgressBar::ProgressSetError()
{
    return false;
}


MainApplication::MainApplication(int& argc, char** argv) :
    QApplication(argc, argv)
{
    m_Options.addOption(QCommandLineOption("lang", "", "language"));
    m_Options.addOption(QCommandLineOption("dir", "", "path"));
    // Command line interface is internal, so using parse() instead of process() to ignore unknown options
    m_Options.parse(arguments());
}

// Returns the language id to be used by the application (specified by --lang, or system locale otherwise)
QString MainApplication::getLocale()
{
    return (m_Options.isSet("lang") ? m_Options.value("lang") : QLocale::system().name());
}

// Returns the start-up directory that will be shown by default in the Open File dialog
QString MainApplication::getInitialDir()
{
    // TODO: Check for elevation
    // linux:restricted
    // linux:root
    // linux: translated dir names
    if (m_Options.isSet("dir"))
        return m_Options.value("dir");

    // Otherwise get the standard system Downloads location
    QStringList downloadDirs = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation);
    if (downloadDirs.size() > 0)
        return downloadDirs.at(0);
    else
        return "";
}

// Returns the fila path passed to the application as command-line parameter
QString MainApplication::getInitialImage()
{
    QStringList args = m_Options.positionalArguments();
    if (args.size() > 0)
        return args.at(0);
    else
        return "";
}

MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow),
    m_ImageFile(""),
    m_ImageSize(0),
    m_LastOpenedDir(""),
    m_IsWriting(false),
    m_EnumFlashDevicesWaiting(false),
    m_ExtProgressBar(this)
{
    ui->setupUi(this);
    // Remove the Context Help button and add the Minimize button to the titlebar
    setWindowFlags((windowFlags() | Qt::CustomizeWindowHint | Qt::WindowMinimizeButtonHint) & ~Qt::WindowContextHelpButtonHint);
    // Disallow to change the dialog height
    setFixedHeight(size().height());
    // Start in the "idle" mode
    hideWritingProgress();
    // Change default open dir
    m_LastOpenedDir = mApp->getInitialDir();
    // Get path to ISO from command line (if supplied)
    QString newImageFile = mApp->getInitialImage();
    if (!newImageFile.isEmpty())
    {
        if (newImageFile.left(7) == "file://")
            newImageFile = QUrl(newImageFile).toLocalFile();
        if (newImageFile != "")
        {
            newImageFile = QDir(newImageFile).absolutePath();
            // Update the default open dir
            m_LastOpenedDir = newImageFile.left(newImageFile.lastIndexOf('/'));
            preprocessImageFile(newImageFile);
        }
    }
    // Load the list of USB flash disks
    enumFlashDevices();
    // TODO: Increase the dialog display speed by showing it with the empty list and enumerating devices
    // in the background (dialog disabled, print "please wait")
    // TODO: Use dialog disabling also for manual refreshing the list
}

MainWindow::~MainWindow()
{
    cleanup();
    delete ui;
}

// Retrieves information about the selected file and displays it in the dialog
void MainWindow::preprocessImageFile(const QString& newImageFile)
{
    QFile f(newImageFile);
    if (!f.open(QIODevice::ReadOnly))
    {
        QMessageBox::critical(
            this,
            ApplicationTitle,
            tr("Failed to open the image file:") + "\n" +
            QDir::toNativeSeparators(newImageFile) + "\n" +
            f.errorString()
        );
        return;
    }
    m_ImageSize = f.size();
    f.close();
    m_ImageFile = newImageFile;
    
    // set ISO file name and path in a text file for verifyimagewriter bash script use
    QString outputFilename = "/tmp/iso.txt";
    QFile outputFile(outputFilename);
    
    outputFile.open(QIODevice::WriteOnly);
    QTextStream outStream(&outputFile);
    outStream << (m_ImageFile);
 
    outputFile.close();
    // end creating text file
    
    ui->imageEdit->setText(QDir::toNativeSeparators(m_ImageFile) + " (" + QString::number(alignNumberDiv(m_ImageSize, DEFAULT_UNIT)) + " " + tr("MB") + ")");
    // Enable the Write button (if there are USB flash disks present)
    ui->writeButton->setEnabled(ui->deviceList->count() > 0);
}

// Frees the GUI-specific allocated resources
void MainWindow::cleanup()
{
    // Delete all the formerly allocated UsbDevice objects attached to the combobox entries
    for (int i = 0; i < ui->deviceList->count(); ++i)
    {
        delete ui->deviceList->itemData(i).value<UsbDevice*>();
    }
}

// The reimplemented dragEnterEvent to inform which incoming drag&drop events are acceptable
void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    // Accept only files with ANSI or Unicode paths (Windows) and URIs (Linux)
    if (event->mimeData()->hasFormat("application/x-qt-windows-mime;value=\"FileName\"") ||
        event->mimeData()->hasFormat("application/x-qt-windows-mime;value=\"FileNameW\"") ||
        event->mimeData()->hasFormat("text/uri-list"))
        event->accept();
}

// The reimplemented dropEvent to process the dropped file
void MainWindow::dropEvent(QDropEvent* event)
{
    QString newImageFile = "";
    QByteArray droppedFileName;

    // First, try to use the Unicode file name
    droppedFileName = event->mimeData()->data("application/x-qt-windows-mime;value=\"FileNameW\"");
    if (!droppedFileName.isEmpty())
    {
        newImageFile = QString::fromWCharArray(reinterpret_cast<const wchar_t*>(droppedFileName.constData()));
    }
    else
    {
        // If failed, use the ANSI name with the local codepage
        droppedFileName = event->mimeData()->data("application/x-qt-windows-mime;value=\"FileName\"");
        if (!droppedFileName.isEmpty())
        {
            newImageFile = QString::fromLocal8Bit(droppedFileName.constData());
        }
        else
        {
            // And, finally, try the URI
            droppedFileName = event->mimeData()->data("text/uri-list");
            if (!droppedFileName.isEmpty())
            {
                // If several files are dropped they are separated by newlines,
                // take the first file
                int newLineIndexLF = droppedFileName.indexOf('\n');
                int newLineIndex = droppedFileName.indexOf("\r\n");
                // Make sure both CRLF and LF are accepted
                if ((newLineIndexLF != -1) && (newLineIndexLF < newLineIndex))
                    newLineIndex = newLineIndexLF;
                if (newLineIndex != -1)
                    droppedFileName = droppedFileName.left(newLineIndex);
                // Decode the file path from percent-encoding
                QUrl url = QUrl::fromEncoded(droppedFileName);
                if (url.isLocalFile())
                    newImageFile = url.toLocalFile();
            }
        }
    }
    if (newImageFile != "")
    {
        // If something was realy received update the information
        preprocessImageFile(newImageFile);
    }
}

// The reimplemented keyPressEvent to display confirmation if user closes the dialog during operation
void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_IsWriting)
    {
        if (QMessageBox::question(this, ApplicationTitle, tr("Writing is in progress, abort it?")) == QMessageBox::No)
            event->ignore();
    }
}

// The reimplemented keyPressEvent to display confirmation if Esc is pressed during operation
// (it normally closes the dialog but does not issue closeEvent for unknown reason)
void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Escape) && m_IsWriting)
    {
        if (QMessageBox::question(this, ApplicationTitle, tr("Writing is in progress, abort it?")) == QMessageBox::No)
            return;
    }
    QDialog::keyPressEvent(event);
}

// Suggests to select image file using the Open File dialog
void MainWindow::openImageFile()
{
    QString newImageFile = QFileDialog::getOpenFileName(this, "", m_LastOpenedDir, tr("Disk Images") + " (*.iso *.bin *.img);;" + tr("All Files") + " (*.*)", NULL, QFileDialog::ReadOnly);
    if (newImageFile != "")
    {
        m_LastOpenedDir = newImageFile.left(newImageFile.lastIndexOf('/'));
        preprocessImageFile(newImageFile);
    }
}

// Schedules reloading the list of USB flash disks to run when possible
void MainWindow::scheduleEnumFlashDevices()
{
    if (m_IsWriting)
        m_EnumFlashDevicesWaiting = true;
    else
        enumFlashDevices();
}

void addFlashDeviceCallback(void* cbParam, UsbDevice* device)
{
    Ui::MainWindow* ui = (Ui::MainWindow*)cbParam;
    ui->deviceList->addItem(device->formatDisplayName(), QVariant::fromValue(device));
}

// Reloads the list of USB flash disks
void MainWindow::enumFlashDevices()
{
    // Remember the currently selected device
    QString selectedDevice = "";
    int idx = ui->deviceList->currentIndex();
    if (idx >= 0)
    {
        UsbDevice* dev = ui->deviceList->itemData(idx).value<UsbDevice*>();
        selectedDevice = dev->m_PhysicalDevice;
    }
    // Remove the existing entries
    cleanup();
    ui->deviceList->clear();
    // Disable the combobox
    // TODO: Disable the whole dialog
    ui->deviceList->setEnabled(false);

    platformEnumFlashDevices(addFlashDeviceCallback, ui);

    // Restore the previously selected device (if present)
    if (selectedDevice != "")
        for (int i = 0; i < ui->deviceList->count(); ++i)
        {
            UsbDevice* dev = ui->deviceList->itemData(i).value<UsbDevice*>();
            if (dev->m_PhysicalDevice == selectedDevice)
            {
                ui->deviceList->setCurrentIndex(i);
                break;
            }
        }
    // Reenable the combobox
    ui->deviceList->setEnabled(true);
    // Update the Write button enabled/disabled state
    ui->writeButton->setEnabled((ui->deviceList->count() > 0) && (m_ImageFile != ""));
    // Update the Clear button enabled/disabled state
    ui->clearButton->setEnabled(ui->deviceList->count() > 0);
    ui->verifyButton->setEnabled((ui->deviceList->count() > 0) && (m_ImageFile != ""));
}

// Starts writing data to the device
void MainWindow::writeToDevice(bool zeroing)
{
    if ((ui->deviceList->count() == 0) || (!zeroing && (m_ImageFile == "")))
        return;
    UsbDevice* selectedDevice = ui->deviceList->itemData(ui->deviceList->currentIndex()).value<UsbDevice*>();
    if (!zeroing && (m_ImageSize > selectedDevice->m_Size))
    {
        QLocale currentLocale;
        QMessageBox::critical(
            this,
            ApplicationTitle,
            tr("The image is larger than your selected device!") + "\n" +
            tr("Image size:") + " " + QString::number(m_ImageSize / DEFAULT_UNIT) + " " + tr("MB") + " (" + currentLocale.toString(m_ImageSize) + " " + tr("b") + ")\n" +
            tr("Disk size:") + " " + QString::number(selectedDevice->m_Size / DEFAULT_UNIT) + " " + tr("MB") + " (" + currentLocale.toString(selectedDevice->m_Size) + " " + tr("b") + ")",
            QMessageBox::Ok
        );
        return;
    }
    if (QMessageBox::warning(
            this,
            ApplicationTitle,
            "<font color=\"red\">" + tr("Warning!") + "</font> " + tr("All existing data on the selected device will be lost!") + "<br>" +
            tr("Are you sure you wish to proceed?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) == QMessageBox::No)
        return;

    showWritingProgress(alignNumberDiv((zeroing ? DEFAULT_UNIT : m_ImageSize), DEFAULT_UNIT));

    ImageWriter* writer = new ImageWriter(zeroing ? "" : m_ImageFile, selectedDevice);
    QThread *writerThread = new QThread(this);

    // Connect start and end signals
    connect(writerThread, &QThread::started, writer, &ImageWriter::writeImage);

    // When writer finishes its job, quit the thread
    connect(writer, &ImageWriter::finished, writerThread, &QThread::quit);

    // Guarantee deleting the objects after completion
    connect(writer, &ImageWriter::finished, writer, &ImageWriter::deleteLater);
    connect(writerThread, &QThread::finished, writerThread, &QThread::deleteLater);

    // If the Cancel button is pressed, inform the writer to stop the operation
    // Using DirectConnection because the thread does not read its own event queue until completion
    connect(ui->cancelButton, &QPushButton::clicked, writer, &ImageWriter::cancelWriting, Qt::DirectConnection);

    // Each time a block is written, update the progress bar
    connect(writer, &ImageWriter::blockWritten, this, &MainWindow::updateProgressBar);

    // Show the message about successful completion on success
    connect(writer, &ImageWriter::success, this, &MainWindow::showSuccessMessage);

    // Show error message if error is sent by the worker
    connect(writer, &ImageWriter::error, this, &MainWindow::showErrorMessage);

    // Silently return back to normal dialog form if the operation was cancelled
    connect(writer, &ImageWriter::cancelled, this, &MainWindow::hideWritingProgress);

    // Now start the writer thread
    writer->moveToThread(writerThread);
    writerThread->start();
}

// Starts writing the image
void MainWindow::writeImageToDevice()
{
    writeToDevice(false);
}

// Clears the selected USB device
void MainWindow::clearDevice()
{
    writeToDevice(true);
    UsbDevice* selectedDevice = ui->deviceList->itemData(ui->deviceList->currentIndex()).value<UsbDevice*>();
    QProcess::startDetached("/usr/bin/umount", {selectedDevice->m_PhysicalDevice});
    QTimer::singleShot(500,this,[=]{QProcess::startDetached("/usr/sbin/mkfs.vfat", {"-F", "32", selectedDevice->m_PhysicalDevice});});
}

// Verify the selected USB device
void MainWindow::verifyDevice()
{
    QProcess::startDetached("/usr/bin/verifyimagewriter", {});
    QMessageBox::information(
        this,
        ApplicationTitle,
        tr("verifying sub-program should have started,") + "<br>" +
        tr("please be aware of the notifications."),
        QMessageBox::Ok);
}

// Updates GUI to the "writing" mode (progress bar shown, controls disabled)
// Also sets the progress bar limits
void MainWindow::showWritingProgress(int maxValue)
{
    m_IsWriting = true;

    // Do not accept dropped files while writing
    setAcceptDrops(false);

    // Disable the main interface
    ui->imageLabel->setEnabled(false);
    ui->imageEdit->setEnabled(false);
    ui->imageSelectButton->setEnabled(false);
    ui->deviceLabel->setEnabled(false);
    ui->deviceList->setEnabled(false);
    ui->deviceRefreshButton->setEnabled(false);

    // Display and customize the progress bar part
    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(maxValue);
    ui->progressBar->setValue(0);
    ui->progressBar->setVisible(true);
    ui->writeButton->setVisible(false);
    ui->clearButton->setVisible(false);
    ui->cancelButton->setVisible(true);
    ui->verifyButton->setVisible(false);

    // Expose the progress bar state to the OS
    m_ExtProgressBar.InitProgressBar(maxValue);
}

// Updates GUI to the "idle" mode (progress bar hidden, controls enabled)
void MainWindow::hideWritingProgress()
{
    m_IsWriting = false;

    // Reenable drag&drop
    setAcceptDrops(true);

    // Enable the main interface
    ui->imageLabel->setEnabled(true);
    ui->imageEdit->setEnabled(true);
    ui->imageSelectButton->setEnabled(true);
    ui->deviceLabel->setEnabled(true);
    ui->deviceList->setEnabled(true);
    ui->deviceRefreshButton->setEnabled(true);

    // Hide the progress bar
    ui->progressBar->setVisible(false);
    ui->writeButton->setVisible(true);
    ui->clearButton->setVisible(true);
    ui->cancelButton->setVisible(false);
    ui->verifyButton->setVisible(true);

    // Send a signal that progressbar is no longer present
    m_ExtProgressBar.DestroyProgressBar();

    // If device list changed during writing update it now
    if (m_EnumFlashDevicesWaiting)
        enumFlashDevices();
}

// Increments the progress bar counter by the specified number
void MainWindow::updateProgressBar(int increment)
{
    int newValue = ui->progressBar->value() + increment;
    ui->progressBar->setValue(newValue);
    m_ExtProgressBar.SetProgressValue(newValue);
}

// Displays the message about successful completion and returns to the "idle" mode
void MainWindow::showSuccessMessage(QString msg)
{
    QMessageBox::information(
        this,
        ApplicationTitle,
        msg
    );
    hideWritingProgress();
}

// Displays the specified error message and returns to the "idle" mode
void MainWindow::showErrorMessage(QString msg)
{
    m_ExtProgressBar.ProgressSetError();
    QMessageBox::critical(
        this,
        ApplicationTitle,
        msg
    );
    hideWritingProgress();
}

#if !defined(Q_OS_LINUX)
#error Unsupported platform!
#endif

int main(int argc, char *argv[])
{
    MainApplication a(argc, argv);

    QString langName = a.getLocale();

    // Load main Qt translation for those languages that do not split into modules
    QTranslator qtTranslator;
    qtTranslator.load("qt_" + langName, QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    a.installTranslator(&qtTranslator);

    // For those languages that come split, load only the base Qt module translation
    QTranslator qtBaseTranslator;
    qtBaseTranslator.load("qtbase_" + langName, QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    a.installTranslator(&qtBaseTranslator);

    // Finally, load the translation of the application itself
    QTranslator appTranslator;
    appTranslator.load("imagewriter-" + langName, QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    a.installTranslator(&appTranslator);

    if (!ensureElevated())
        return 1;

    MainWindow w;
    w.show();

    UsbDeviceMonitor deviceMonitor;
    deviceMonitor.startMonitoring();

    // When device changing event comes, refresh the list of USB flash disks
    // Using QueuedConnection to avoid delays in processing the message
    QObject::connect(&deviceMonitor, &UsbDeviceMonitor::deviceChanged, &w, &MainWindow::scheduleEnumFlashDevices, Qt::QueuedConnection);

    return a.exec();
}

#include "common.h"
#include "mainwindow.h"
#include "usbdevice.h"

#include <QFile>
#include <QStringList>
#include <QMessageBox>
#include <QDir>
#include <QRegularExpression>
#include <QProcess>
#include <QStandardPaths>

// Gets the contents of the specified file
// Input:
//  fileName - path to the file to read
// Returns:
//  the file contents or empty string if an error occurred
QString readFileContents(const QString& fileName)
{
    QFile f(fileName);
    if (!f.open(QFile::ReadOnly))
        return "";
    QString ret = f.readAll();
    f.close();
    return ret;
}

bool platformEnumFlashDevices(AddFlashDeviceCallbackProc callback, void* cbParam)
{
    // Using /sys/bus/usb/devices directory contents for enumerating the USB devices
    //
    // Details:
    // Take the devices which have <device>/bInterfaceClass contents set to "08" (storage device).
    //
    // 1. To get the user-friendly name we need to read the <manufacturer> and <product> files
    //    of the parent device (the parent device is the one with less-specified name, e.g. "2-1" for "2-1:1.0").
    //
    // 2. The block device name can be found by searching the contents of the following subdirectory:
    //      <device>/host*/target*/<scsi-device-name>/block/
    //    where * is a placeholder, and <scsi-device-name> starts with the same substring that "target*" ends with.
    //    For example, this path may look like follows:
    //      /sys/bus/usb/devices/1-1:1.0/host4/target4:0:0/4:0:0:0/block/
    //    This path contains the list of block devices by their names, e.g. sdc, which gives us /dev/sdc.
    //
    // 3. And, finally, for the device size we multiply .../block/sdX/size (the number of sectors) with
    //    .../block/sdX/queue/logical_block_size (the sector size).

    // Start with enumerating all the USB devices
    QString usbDevicesRoot = "/sys/bus/usb/devices";
    QDir dirList(usbDevicesRoot);
    QStringList usbDevices = dirList.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (int deviceIdx = 0; deviceIdx < usbDevices.size(); ++deviceIdx)
    {
        QDir deviceDir = dirList;
        if (!deviceDir.cd(usbDevices[deviceIdx]))
            continue;

        // Skip devices with wrong interface class
        if (readFileContents(deviceDir.absoluteFilePath("bInterfaceClass")) != "08\n")
            continue;

        // Search for "host*" entries and process them
        QStringList hosts = deviceDir.entryList(QStringList("host*"));
        for (int hostIdx = 0; hostIdx < hosts.size(); ++hostIdx)
        {
            QDir hostDir = deviceDir;
            if (!hostDir.cd(hosts[hostIdx]))
                continue;

            // Search for "target*" entries and process them
            QStringList targets = hostDir.entryList(QStringList("target*"));
            for (int targetIdx = 0; targetIdx < targets.size(); ++targetIdx)
            {
                QDir targetDir = hostDir;
                if (!targetDir.cd(targets[targetIdx]))
                    continue;

                // Remove the "target" part and append "*" to search for appropriate SCSI devices
                QStringList scsiTargets = targetDir.entryList(QStringList(targets[targetIdx].mid(6) + "*"));
                for (int scsiTargetIdx = 0; scsiTargetIdx < scsiTargets.size(); ++scsiTargetIdx)
                {
                    QDir scsiTargetDir = targetDir;
                    if (!scsiTargetDir.cd(scsiTargets[scsiTargetIdx] + "/block"))
                        continue;

                    // Read the list of block devices and process them
                    QStringList blockDevices = scsiTargetDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                    for (int blockDeviceIdx = 0; blockDeviceIdx < blockDevices.size(); ++blockDeviceIdx)
                    {
                        // Create the new UsbDevice object to bind information to the listbox entry
                        UsbDevice* deviceData = new UsbDevice;

                        // Use the block device name as both physical device and displayed volume name
                        deviceData->m_PhysicalDevice = "/dev/" + blockDevices[blockDeviceIdx];
                        deviceData->m_Volumes << deviceData->m_PhysicalDevice;

                        // Get the device size
                        quint64 blocksNum = readFileContents(scsiTargetDir.absoluteFilePath(blockDevices[blockDeviceIdx] + "/size")).toULongLong();
                        // The size is counted in logical blocks (tested with 4K-sector HDD)
                        deviceData->m_SectorSize = readFileContents(scsiTargetDir.absoluteFilePath(blockDevices[blockDeviceIdx] + "/queue/logical_block_size")).toUInt();
                        if (deviceData->m_SectorSize == 0)
                            deviceData->m_SectorSize = 512;
                        deviceData->m_Size = blocksNum * deviceData->m_SectorSize;

                        // Get the user-friendly name for the device by reading the parent device fields
                        QString usbParentDevice = usbDevices[deviceIdx];
                        usbParentDevice.replace(QRegularExpression("^(\\d+-\\d+):.*$"), "\\1");
                        usbParentDevice.prepend(usbDevicesRoot + "/");
                        // TODO: Find out how to get more "friendly" name (for SATA-USB connector it shows the bridge
                        // device name instead of the disk drive name)
                        deviceData->m_VisibleName = (
                                readFileContents(usbParentDevice + "/manufacturer").trimmed() +
                                " " +
                                readFileContents(usbParentDevice + "/product").trimmed()
                            ).trimmed();

                        // The device information is now complete, append the entry
                        callback(cbParam, deviceData);
                    }
                }
            }
        }
    }

    return true;
}

bool ensureElevated()
{
    // If we already have root privileges do nothing
    uid_t uid = getuid();
    if (uid == 0)
        return true;

    // Search for known GUI su-applications.
    // The list is priority-ordered. If there are native su-applications present,
    // using the first such program. Otherwise, using just the first program that is present.
    QList<SuProgram*> suPrograms = { new pkSu(), new XdgSu(), new BeeSu(), new KdeSu(), new GkSu() };
    SuProgram* suProgram = NULL;
    for (int i = 0; i < suPrograms.size(); ++i)
    {
        // Skip missing su-apps
        if (!suPrograms[i]->isPresent())
            continue;

        if (suPrograms[i]->isNative())
        {
            // If we found a native su-application - using it as preferred and stop searching
            suProgram = suPrograms[i];
            break;
        }
        else
        {
            // If not native, and no other su-application was found - using it, but continue searching,
            // in case a native app will appear down the list
            if (suProgram == NULL)
                suProgram = suPrograms[i];
        }
    }
    if (suProgram == NULL)
    {
        QMessageBox::critical(
            NULL,
            ApplicationTitle,
            "<font color=\"red\">" + QObject::tr("Error!") + "</font> " + QObject::tr("No appropriate su-application found!") + "<br>" +
            QObject::tr("Please, restart the program with root privileges."),
            QMessageBox::Ok
        );
        return false;
    }

    // Prepare the list of arguments and restart ourselves using the su-application found
    QStringList args;
    // First comes our own executable
    args << mApp->applicationFilePath();
    // We need to explicitly pass language and initial directory so that the new instance
    // inherited the current user's parameters rather than root's
    QString argLang = mApp->getLocale();
    if (!argLang.isEmpty())
        args << "--lang=" + argLang;
    QString argDir = mApp->getInitialDir();
    if (!argDir.isEmpty())
        args << "--dir=" + argDir;
    // Finally, if image file was supplied, append it as well
    QString argImage = mApp->getInitialImage();
    if (!argImage.isEmpty())
        args << argImage;
    // And now try to take off with all this garbage
    suProgram->restartAsRoot(args);

    // Something went wrong, we should have never returned! Cleanup and return error
    for (int i = 0; i < suPrograms.size(); ++i)
        delete suPrograms[i];
    QMessageBox::critical(
        NULL,
        ApplicationTitle,
        "<font color=\"red\">" + QObject::tr("Error!") + "</font> " + QObject::tr("Failed to restart with root privileges! (Error code: %1)").arg(errno) + "<br>" +
        QObject::tr("Please, restart the program with root privileges."),
        QMessageBox::Ok
    );
    return false;
}

// Copied from private Qt implementation at qtbase/src/platformsupport/services/genericunix/qgenericunixservices.cpp
static QString detectDesktopEnvironment()
{
    const QString xdgCurrentDesktop = qgetenv("XDG_CURRENT_DESKTOP");
    if (!xdgCurrentDesktop.isEmpty())
        return xdgCurrentDesktop.toUpper(); // KDE, GNOME, UNITY, LXDE, MATE, XFCE...

    // Classic fallbacks
    if (!qEnvironmentVariableIsEmpty("KDE_FULL_SESSION"))
        return "KDE";
    if (!qEnvironmentVariableIsEmpty("GNOME_DESKTOP_SESSION_ID"))
        return "GNOME";

    // Fallback to checking $DESKTOP_SESSION (unreliable)
    const QString desktopSession = qgetenv("DESKTOP_SESSION");
    if (desktopSession == "gnome")
        return "GNOME";
    if (desktopSession == "xfce")
        return "XFCE";

    return desktopSession;
}

SuProgram::SuProgram(QString binaryName, QStringList suArgs, bool splitArgs)
    : m_binaryName(binaryName)
    , m_suArgs(suArgs)
    , m_splitArgs(splitArgs)
{
    m_binaryPath = QStandardPaths::findExecutable(m_binaryName);
}

pkSu::pkSu()
    : SuProgram("pkexec", {"env", "DISPLAY=" + qgetenv("DISPLAY"), "XAUTHORITY=" + qgetenv("XAUTHORITY") }, true)
{
}

XdgSu::XdgSu()
    : SuProgram("xdg-su", {"-c"}, false)
{
}

KdeSu::KdeSu()
    : SuProgram("kdesu", {"-c"}, false)
{
    // Extract from man:
    //   Since <kdesu> is no longer installed in <$(kde4-config --prefix)/bin> but in <kde4-config --path libexec>
    //   and therefore not in your <Path>, you have to use <$(kde4-config --path libexec)kdesu> to launch <kdesu>.
    // Now, what we do: first, simple PATH check is performed in SuProgram constructor. Then we try to find kdesu
    // according to the KDE documentation and, if found, overwrite the path with the new one.
    QProcess kdeConfig;
    kdeConfig.start("kde4-config", {"--path", "libexec"}, QIODevice::ReadOnly);
    kdeConfig.waitForFinished(3000);
    QString kdesuPath = kdeConfig.readAllStandardOutput().trimmed();
    if (!kdesuPath.isEmpty())
    {
        kdesuPath += (kdesuPath.endsWith('/') ? "" : "/");
        kdesuPath += "kdesu";
        if (QFile::exists(kdesuPath) && QFile::permissions(kdesuPath).testFlag(QFileDevice::ExeUser))
            m_binaryPath = kdesuPath;
    }
}

GkSu::GkSu()
    : SuProgram("gksu", {}, false)
{
}

BeeSu::BeeSu()
    : SuProgram("beesu", {}, true)
{
}


bool SuProgram::isPresent() const
{
    return (!m_binaryPath.isEmpty());
}

bool pkSu::isNative() const
{
    // pkexec is considered a universal tool
    return true;
}

bool XdgSu::isNative() const
{
    // xdg-su is also considered a universal tool
    return true;
}

bool KdeSu::isNative() const
{
    // kdesu is native only in KDE
    const QString desktopEnvironment = detectDesktopEnvironment();
    return (desktopEnvironment == "KDE");
}

bool GkSu::isNative() const
{
    // gksu is native for GTK-based DEs; however, for Qt-based DEs (like LXQt) there is no alternative either
    const QString desktopEnvironment = detectDesktopEnvironment();
    return (!desktopEnvironment.isEmpty() && (desktopEnvironment != "KDE"));
}

bool BeeSu::isNative() const
{
    // beesu is developed for Fedora/RedHat, let's consider it native there
    QFile redhatRelease("/etc/redhat-release");
    if (!redhatRelease.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QString redhatInfo = redhatRelease.readAll();
    return redhatInfo.contains("Red Hat", Qt::CaseInsensitive) || redhatInfo.contains("Fedora", Qt::CaseInsensitive);
}


void SuProgram::restartAsRoot(const QStringList& args)
{
    if (m_binaryPath.isEmpty())
        return;

    int i;

    // For execv() we need the list of char* arguments; using QByteArrays as temporary storage
    // Store QByteArray objects explicitly to make sure they live long enough, so that their data()'s were valid until execv() call
    QList<QByteArray> argsBA;

    // First comes the application being started (su-application) with all its arguments (if any)
    argsBA << m_binaryPath.toUtf8();
    for (i = 0; i < m_suArgs.size(); ++i)
        argsBA << m_suArgs[i].toUtf8();
    // Now append the passed arguments
    // Depending on the su-application, we may need to either pas them separately, or space-join them into one single argument word
    if (m_splitArgs)
    {
        for (i = 0; i < args.size(); ++i)
            argsBA << args[i].toUtf8();
    }
    else
    {
        QString joined = '\'' + args[0] + '\'';
        for (i = 1; i < args.size(); ++i)
            joined += " '" + args[i] + "'";
        argsBA << joined.toUtf8();
    }

    // Convert arguments into char*'s and append NULL element
    int argsNum = argsBA.size();
    char** argsBin = new char*[argsNum + 1];
    for (i = 0; i < argsNum; ++i)
        argsBin[i] = argsBA[i].data();
    argsBin[argsNum] = NULL;

    // Replace ourselves with su-application
    execv(argsBin[0], argsBin);

    // Something went wrong, we should have never returned! Cleaning up
    delete[] argsBin;
}

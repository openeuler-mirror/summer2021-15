#ifndef COMMON_H
#define COMMON_H

////////////////////////////////////////////////////////////////////////////////
// This file contains some commonly-used constants and function declarations


#include <QObject>
#include <QString>
#include <QtGlobal>

#include <type_traits>

#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <libudev.h>

// Application name used for titles in messageboxes
#define ApplicationTitle QString(QObject::tr("Image Writer"))

class UsbDevice;


// Default unit to be used when displaying file/device sizes (MB)
const quint64 DEFAULT_UNIT = 1048576;

// Pointer to correctly typed application instance
#define mApp (static_cast<MainApplication*>qApp)

// Returns the number of blocks required to contain some number of bytes
// Input:
//  T      - any integer type
//  val    - number of bytes
//  factor - size of the block
// Returns:
//  the number of blocks of size <factor> required for <val> to fit in
template <typename T> T alignNumberDiv(T val, T factor)
{
    static_assert(std::is_integral<T>::value, "Only integer types are supported!");
    return ((val + factor - 1) / factor);
}

// Returns the total size of blocks required to contain some number of bytes
// Input:
//  T      - any integer type
//  val    - number of bytes
//  factor - size of the block
// Returns:
//  the total size of blocks of size <factor> required for <val> to fit in
template <typename T> T alignNumber(T val, T factor)
{
    static_assert(std::is_integral<T>::value, "Only integer types are supported!");
    return alignNumberDiv(val, factor) * factor;
}

// Gets the contents of the specified file
// Input:
//  fileName - path to the file to read
// Returns:
//  the file contents or empty string if an error occurred
QString readFileContents(const QString& fileName);

// Callback function type for platformEnumFlashDevices (see below)
// Input:
//  cbParam        - parameter passed to the enumeration function
//  DeviceVendor   - vendor of the USB device
//  DeviceName     - name of the USB device
//  PhysicalDevice - OS-specific path to the physical device
//  Volumes        - list of volumes this device contains
//  NumVolumes     - number of volumes in the list
//  Size           - size of the disk in bytes
//  SectorSize     - sector size of the device
// Returns:
//  nothing
typedef void (*AddFlashDeviceCallbackProc)(void* cbParam, UsbDevice* device);

// Performs enumeration of USB flash disks and calls the callback
// function for adding these devices into the application GUI structure
// Input:
//  callback - callback function to be called for each new device
//  cbParam  - parameter to be passed to this callback function
// Returns:
//  true if enumeration completed successfully, false otherwise
bool platformEnumFlashDevices(AddFlashDeviceCallbackProc callback, void* cbParam);

// Checks the application privileges and if they are not sufficient, restarts
// itself requesting higher privileges
// Input:
//  appPath - path to the application executable
// Returns:
//  true if already running elevated
//  false if error occurs
//  does not return if elevation request succeeded (the current instance terminates)
bool ensureElevated();

class SuProgram
{
protected:
    // Name of the su-application
    const QString m_binaryName;
    // Full path to the executable (if present)
    QString m_binaryPath;
    // Additional arguments
    const QStringList m_suArgs;
    // Whether it accepts target command line as separate arguments or single argument
    const bool m_splitArgs;

    // Constructor
    SuProgram(QString binaryName, QStringList suArgs, bool splitArgs);

public:
    // Destructor
    virtual ~SuProgram() {}
    // Check if the program is present in the system; by default searching in PATH is used
    virtual bool isPresent() const;
    // Check whether the su-application is native to the current desktop environment
    virtual bool isNative() const = 0;
    // Restarts the current application with the specified arguments using the GUI su program
    // Returns only if error occurred (execv() is used)
    virtual void restartAsRoot(const QStringList& args);
};


// Derivative classes for specific su-applications

class pkSu : public SuProgram
{
public:
    pkSu();
    virtual ~pkSu() {}
    virtual bool isNative() const;
};

class XdgSu : public SuProgram
{
public:
    XdgSu();
    virtual ~XdgSu() {}
    virtual bool isNative() const;
};

class KdeSu : public SuProgram
{
public:
    KdeSu();
    virtual ~KdeSu() {}
    virtual bool isNative() const;
};

class GkSu : public SuProgram
{
public:
    GkSu();
    virtual ~GkSu() {}
    virtual bool isNative() const;
};

class BeeSu : public SuProgram
{
public:
    BeeSu();
    virtual ~BeeSu() {}
    virtual bool isNative() const;
};

#endif // COMMON_H

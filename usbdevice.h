#ifndef USBDEVICE_H
#define USBDEVICE_H

////////////////////////////////////////////////////////////////////////////////
// Class for storing information about USB flash disk

#include "common.h"

#include <QStringList>
#include <QObject>
#include <QSocketNotifier>
#include <QAbstractNativeEventFilter>

class UsbDevice
{
public:
    UsbDevice() :
        m_VisibleName(QObject::tr("Unknown Device")),
        m_Volumes(),
        m_Size(0),
        m_SectorSize(512),
        m_PhysicalDevice("") {}

    // Formats the device description for GUI
    // The format is: "<volume(s)> - <user-friendly name> (<size in megabytes>)"
    QString formatDisplayName() const {
        return ((m_Volumes.size() == 0) ? QObject::tr("<unmounted>") : m_Volumes.join(", ")) + " - " + m_VisibleName + " (" + QString::number(alignNumberDiv(m_Size, DEFAULT_UNIT)) + " " + QObject::tr("MB") + ")";
    }

    // User-friendly name of the device
    QString     m_VisibleName;
    // List of mounted volumes from this device
    QStringList m_Volumes;
    // Size of the device
    quint64     m_Size;
    // Sector size
    quint32     m_SectorSize;
    // System name of the physical disk
    QString     m_PhysicalDevice;
};

Q_DECLARE_METATYPE(UsbDevice*)

class UsbDeviceMonitor;
class UsbDeviceMonitorPrivate : public QObject
{
    Q_OBJECT

public:
    explicit UsbDeviceMonitorPrivate(QObject *parent = 0);
    virtual ~UsbDeviceMonitorPrivate();

    UsbDeviceMonitor* q_ptr;

    // Handle to dynamically loaded udev library
    void* m_udevLib;
    // udev library context
    struct udev* m_udev;
    // udev device monitor handle
    struct udev_monitor* m_udevMonitor;
    // Watcher for udev monitor socket
    QSocketNotifier* m_udevNotifier;

public slots:
    // Processes udev socket notification
    void processUdevNotification(int socket);
};

class UsbDeviceMonitor : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

protected:
    UsbDeviceMonitorPrivate* const d_ptr;

public:
    explicit UsbDeviceMonitor(QObject *parent = 0);
    ~UsbDeviceMonitor();
    
    // Implements QAbstractNativeEventFilter interface for processing WM_DEVICECHANGE messages (Windows)
    bool nativeEventFilter(const QByteArray& eventType, void* message, long* result);

protected:
    // Closes handles and frees resources
    void cleanup();

signals:
    // Emitted when device change notification arrives
    void deviceChanged();

public slots:
    // Initializes monitoring for USB devices
    bool startMonitoring();
};

#endif // USBDEVICE_H

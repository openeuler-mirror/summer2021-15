////////////////////////////////////////////////////////////////////////////////
// Implementation of PhysicalDevice


#include "physicaldevice.h"

PhysicalDevice::PhysicalDevice(const QString& name) :
    QFile(name)
{
}

// Opens the selected device in WriteOnly mode
bool PhysicalDevice::open()
{
#if defined(Q_OS_LINUX)
    // Simply use QFile, it works fine in Linux
    // TODO: Use system call open with O_DIRECT
    return QFile::open(QIODevice::WriteOnly);
#else
    return false;
#endif
}

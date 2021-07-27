#ifndef PHYSICALDEVICE_H
#define PHYSICALDEVICE_H

////////////////////////////////////////////////////////////////////////////////
// Class implementing write-only physical device


#include <QFile>

#include "common.h"

class PhysicalDevice : public QFile
{
    Q_OBJECT
public:
    PhysicalDevice(const QString& name);

    // Opens the selected device in WriteOnly mode
    virtual bool open();
};

#endif // PHYSICALDEVICE_H

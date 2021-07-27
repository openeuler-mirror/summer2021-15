#include "common.h"

#include <QFile>
#include <QStringList>

////////////////////////////////////////////////////////////////////////////////
// Implementation of the non-template cross-platform functions from common.h
////////////////////////////////////////////////////////////////////////////////

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

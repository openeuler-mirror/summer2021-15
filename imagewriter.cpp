////////////////////////////////////////////////////////////////////////////////
// Implementation of ImageWriter


#include <QFile>

#include "common.h"
#include "imagewriter.h"
#include "physicaldevice.h"

ImageWriter::ImageWriter(const QString& ImageFile, UsbDevice* Device, QObject *parent) :
    QObject(parent),
    m_Device(Device),
    m_ImageFile(ImageFile),
    m_CancelWriting(false)
{
}

// The main method that writes the image
void ImageWriter::writeImage()
{
    const qint64 TRANSFER_BLOCK_SIZE = 1024 * 1024;
    void* buffer = NULL;

    bool isError = false;
    bool cancelRequested = false;
    bool zeroing = (m_ImageFile == "");

    // Using try-catch for processing errors
    // Invalid values are used for indication non-initialized objects;
    // after the try-catch block all the initialized objects are freed
    try
    {
        buffer = malloc(TRANSFER_BLOCK_SIZE);
        if (buffer == NULL)
            throw tr("Failed to allocate memory for buffer.");

        QFile imageFile;
        if (zeroing)
        {
            // Prepare zero-filled buffer
            memset(buffer, 0, TRANSFER_BLOCK_SIZE);
        }
        else
        {
            // Open the source image file for reading
            imageFile.setFileName(m_ImageFile);
            if (!imageFile.open(QIODevice::ReadOnly))
                throw tr("Failed to open the image file:") + "\n" + imageFile.errorString();
        }

        // Unmount volumes that belong to the selected target device
        // TODO: Check first if they are used and show warning
        // (problem: have to show request in the GUI thread and return reply back here)
        QStringList errMessages;

        if (errMessages.size() > 0)
            throw errMessages.join("\n\n");

        // Open the target USB device for writing and lock it
        PhysicalDevice deviceFile(m_Device->m_PhysicalDevice);
        if (!deviceFile.open())
            throw tr("Failed to open the target device:") + "\n" + deviceFile.errorString();

        qint64 readBytes;
        qint64 writtenBytes;
        // Start reading/writing cycle
        for (;;)
        {
            if (zeroing)
            {
                readBytes = TRANSFER_BLOCK_SIZE;
            }
            else
            {
                if ((readBytes = imageFile.read(static_cast<char*>(buffer), TRANSFER_BLOCK_SIZE)) <= 0)
                    break;
            }
            // Align the number of bytes to the sector size
            readBytes = alignNumber(readBytes, (qint64)m_Device->m_SectorSize);
            writtenBytes = deviceFile.write(static_cast<char*>(buffer), readBytes);
            if (writtenBytes < 0)
                throw tr("Failed to write to the device:") + "\n" + deviceFile.errorString();
            if (writtenBytes != readBytes)
                throw tr("The last block was not fully written (%1 of %2 bytes)!\nAborting.").arg(writtenBytes).arg(readBytes);

            // In Linux/MacOS the USB device is opened with buffering. Using forced sync to validate progress bar.
            // For unknown reason, deviceFile.flush() does not work as intended here.
            fsync(deviceFile.handle());

            // Inform the GUI thread that next block was written
            // TODO: Make sure that when TRANSFER_BLOCK_SIZE is not a multiple of DEFAULT_UNIT
            // this still works or at least fails compilation
            emit blockWritten(TRANSFER_BLOCK_SIZE / DEFAULT_UNIT);

            // Check for the cancel request (using temporary variable to avoid multiple unlock calls in the code)
            m_Mutex.lock();
            cancelRequested = m_CancelWriting;
            m_Mutex.unlock();
            if (cancelRequested)
            {
                // The cancel request was issued
                emit cancelled();
                break;
            }
            if (zeroing)
            {
                // In zeroing mode only write 1 block - 1 MB is enough to clear both MBR and GPT
                break;
            }
        }
        if (!zeroing)
        {
            if (readBytes < 0)
                throw tr("Failed to read the image file:") + "\n" + imageFile.errorString();
            imageFile.close();
        }
        deviceFile.close();
    }
    catch (QString msg)
    {
        // Something went wrong :-(
        emit error(msg);
        isError = true;
    }

    if (buffer != NULL)
        free(buffer);

    // If no errors occurred and user did not stop the operation, it means everything went fine
    if (!isError && !cancelRequested)
    {
        emit success(
            tr("The operation completed successfully.") +
            "<br><br>" +
            (zeroing ? tr("Now your device has been formatted into FAT32.") : tr("To be able to store data on this device again, please use the <b>Clear</b> button.")+
            "<br><br>" +
            tr("To check the integrity of the USB use the <b>Verify</b> button."))
        );
    }

    // In any case the operation is finished
    emit finished();
}

// Implements reaction to the cancel request from user
void ImageWriter::cancelWriting()
{
    m_Mutex.lock();
    m_CancelWriting = true;
    m_Mutex.unlock();
}

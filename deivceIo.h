#ifndef PORT_H
#define PORT_H

#include "serial/serial.h"
#include <QThread>
#include <QProcess>
#include <QDebug>
#include <QEventLoop>

class Port : public QThread
{
public:
    enum pDirection
    {
        READ,
        WRITE
    };

public:
    explicit Port(serial::Serial *io, QObject *parent = 0):
        QThread(parent),
        io(io)
    {
        readlen = 0;
    }
    ~Port()
    {

    }

    bool read8bit(uint8_t *data)
    {
        direction = READ;
        error = false;
        readdata = (char*)data;
        readlen = sizeof(uint8_t);

        start();
        wait();

        return !error;
    }
    bool write8bit(uint8_t data)
    {
        direction = WRITE;
        writedata = (char*)&data;
        writelen = sizeof(uint8_t);
        readlen = 0;
        error = false;

        start();
        wait();

        return !error;
    }

    bool read16bit(uint16_t *data)
    {
        direction = READ;
        error = false;
        readdata = (char*)data;
        readlen = sizeof(uint16_t);

        start();
        wait();

        return !error;
    }
    bool write16bit(uint16_t data)
    {
        direction = WRITE;
        writedata = (char*)&data;
        writelen = sizeof(uint16_t);
        readlen = 0;
        error = false;

        start();
        wait();

        return !error;
    }

    bool read32bit(uint32_t *data)
    {
        direction = READ;
        error = false;
        readdata = (char*)data;
        readlen = sizeof(uint32_t);

        start();
        wait();

        return !error;
    }
    bool write32bit(uint32_t data)
    {
        direction = WRITE;
        writedata = (char*)&data;
        writelen = sizeof(uint32_t);
        readlen = 0;
        error = false;

        start();
        wait();

        return !error;
    }

    bool read64bit(uint64_t *data)
    {
        direction = READ;
        error = false;
        readdata = (char*)data;
        readlen = sizeof(uint64_t);

        start();
        wait();

        return !error;
    }
    bool write64bit(uint64_t data)
    {
        direction = WRITE;
        writedata = (char*)&data;
        writelen = sizeof(uint64_t);
        readlen = 0;
        error = false;

        start();
        wait();

        return !error;
    }

    int readptr(void *data, int len)
    {
        direction = READ;
        error = false;
        readdata = (char*)data;
        readlen = len;

        start();
        wait();

        if (error)
            return -1;
        return readlen;
    }
    bool writeptr(void *data, int len)
    {
        direction = WRITE;
        writedata = (char*)data;
        writelen = len;
        readlen = 0;
        error = false;

        start();
        wait();

        return !error;
    }

protected:
    void run() override
    {
        switch (direction)
        {
        case READ:
        {
            try
            {
                int total = 0;
                while (true)
                {
                    int len = io->read((uint8_t*)readdata+total, readlen);

                    total += len;
                    readlen -= len;

                    if (readlen == 0)
                        break;

                    if (!io->available())
                        break;
                }
                readlen = total;
            }
            catch (std::exception e)
            {
                errlog = QString::fromUtf8(e.what());
                error = true;
            }
            catch (QString err)
            {
                errlog = err;
                error = true;
            }
        } break;
        case WRITE:
        {
            try
            {
                int len = io->write((uint8_t*)writedata, writelen);
                if (len != writelen)
                    throw QString("failed to write data");
            }
            catch(std::exception e)
            {
                errlog = QString::fromUtf8(e.what());
                error = true;
            }
            catch(QString err)
            {
                errlog = err;
                error = true;
            }
        } break;
        }
    }

private:
    pDirection  direction;

    int         readlen,
                writelen;
    bool        error;
    char        *readdata = 0,
                *writedata = 0;

    serial::Serial *io;
    QString errlog;

};
class Process : public QProcess
{
    Q_OBJECT
public:
    enum pType
    {
        LIST,
        INSTALL,
        UNINSTALL
    };

public:
    explicit Process(QString exePath, QObject *parent = 0) :
        QProcess(parent),
        path(exePath)
    {
        connect(this, SIGNAL(readyReadStandardOutput()), this, SLOT(pReadOutput()));
        connect(this, SIGNAL(readyReadStandardError()), this, SLOT(pReadError()));
        connect(this, SIGNAL(finished(int)), this, SLOT(pFinished(int)));
    }
    ~Process()
    {

    }

    bool installDriver()
    {
        errlog.clear();
        outputData.clear();
        driverClassName.clear();
        error = false;
        type = LIST;
        start(path, QStringList() << "list" << "--class=Ports");
        lock.exec();

        if (error)
            return false;

        if (driverClassName.isEmpty())
        {
            errlog = "Mediatek driver class name not found";
            return false;
        }

        errlog.clear();
        outputData.clear();
        error = false;
        type = INSTALL;
        start(path, QStringList() << "install" << QString("--device=%1").arg(driverClassName.replace("&",".")));
        lock.exec();

        return !error;
    }
    bool uninstallDriver()
    {
        errlog.clear();
        outputData.clear();
        driverClassName.clear();
        error = false;
        type = LIST;
        start(path, QStringList() << "list" << "--class=Ports");
        lock.exec();

        if (error)
            return false;

        if (driverClassName.isEmpty())
        {
            errlog = "Mediatek driver class name not found";
            return false;
        }

        errlog.clear();
        outputData.clear();
        error = false;
        type = UNINSTALL;
        start(path, QStringList() << "uninstall" << QString("--device=%1").arg(driverClassName.replace("&",".")));
        lock.exec();

        return !error;
    }

    QString getErrLog()
    {
        return errlog;
    }

private slots:
    void pReadOutput()
    {
        outputData.append(readAllStandardOutput());
    }
    void pReadError()
    {
        errlog.append(readAllStandardError());
        error = true;
    }

    void pFinished(int)
    {
        if (error)
        {
            lock.quit();
            return;
        }

        switch (type)
        {

        case LIST:
        {
            QStringList list;
            QTextStream st(outputData);
            while (!st.atEnd())
            {
                QString line = st.readLine();
                if (!line.contains("vid_0e8d&pid_0003", Qt::CaseInsensitive))
                    continue;
                list << line;
            }

            if (list.isEmpty())
            {
                errlog = "Mediatek driver not detected";
                error = true;
                lock.quit();
                return;
            }

            for (QString line : list)
            {
                if (line.contains("&rev", Qt::CaseInsensitive))
                {
                    QStringList llist = line.split(' ', QString::SkipEmptyParts);
                    for (QString lline : llist)
                    {
                        if (lline.startsWith("usb\\vid_", Qt::CaseInsensitive))
                            driverClassName = lline.trimmed();
                    }
                }
            }

            lock.quit();
        } break;

        case INSTALL:
        {
            bool success = false;

            QStringList list;
            QTextStream st(outputData);
            while (!st.atEnd())
            {
                QString line = st.readLine();
                if (line.contains("inserting", Qt::CaseInsensitive))
                    success = true;
                if (line.contains("creating", Qt::CaseInsensitive))
                    success = true;
                if (line.contains("restarting", Qt::CaseInsensitive))
                    success = true;
                if (line.contains("starting", Qt::CaseInsensitive))
                    success = true;
            }

            if (!success)
            {
                errlog = "failed to install driver";
                error = true;
            }
            lock.quit();
        } break;

        case UNINSTALL:
        {
            bool success = false;

            QStringList list;
            QTextStream st(outputData);
            while (!st.atEnd())
            {
                QString line = st.readLine();
                if (line.contains("removing", Qt::CaseInsensitive))
                    success = true;
                if (line.contains("deleting", Qt::CaseInsensitive))
                    success = true;
                if (line.contains("restarting", Qt::CaseInsensitive))
                    success = true;
                if (line.contains("stopping", Qt::CaseInsensitive))
                    success = true;
            }

            if (!success)
            {
                errlog = "failed to uninstall driver";
                error = true;
            }
            lock.quit();

        } break;

        }
    }

private:
    QString     path;
    pType       type;
    bool        error;
    QEventLoop  lock;
    QByteArray  outputData;
    QString     errlog,
                driverClassName;
};

#endif // PORT_H

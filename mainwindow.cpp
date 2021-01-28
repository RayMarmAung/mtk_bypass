#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QLineEdit>

#include "UsbDevice.h"
#include "lusb0_usb.h"
#include "serial/serial.h"
#include "deivceIo.h"
#include "About.h"
#include "sponsor.h"

inline bool isWow64Process()
{
    typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
    BOOL bIsWow64 = FALSE;
    LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process");

    if( NULL != fnIsWow64Process )
    {
        if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
            bIsWow64 = FALSE;
    }
    return bIsWow64;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setFixedSize(size());

    QLineEdit *le = new QLineEdit(ui->cbSoc);
    ui->cbSoc->setLineEdit(le);
    le->setReadOnly(true);

    QDir dir(":/payload");
    ui->cbSoc->addItem("[AUTO]");
    for (QFileInfo f : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks))
    {
        if (f.baseName().contains("generic"))
            continue;
        ui->cbSoc->addItem(f.baseName().remove("_payload").toUpper(), f.absoluteFilePath());    
    }
    ui->cbSoc->setCurrentText("[AUTO]");

    listen = new Listener();
    {
        connect(listen, SIGNAL(connected(Device*)), this, SLOT(getPort(Device*)));
        connect(listen, SIGNAL(getError(QString)), this, SLOT(getPortError(QString)));
    }
    io = new serial::Serial();

    setLog(QStringList() << "WELCOME TO MCT MEDIATEK BYPASS TOOL$b$c560fa3", false);
    setLog(QStringList() << "\n[NO NEED TO INSTALL PYTHON & LIBUSB DRIVER]$b$c560fa3", false);
    setLog(QStringList() << "\n\nDON'T FORGET TO SPONSOR US\n"
                            "      IF YOU ENJOY JUST LIKE THIS FREE TOOL$b$caa0000", false);
    setLog(QStringList() << "\n\nSUPPORTED MODELS:$b$c00a000", false);
    for (int i = 0; i < ui->cbSoc->count(); i++)
    {
        QString model = ui->cbSoc->itemText(i);
        if (model.contains("auto", Qt::CaseInsensitive))
            continue;
        setLog(QStringList() << QString("\n  ● %1$b$c0000a0").arg(model), false);
    }
}
MainWindow::~MainWindow()
{
    if (listen)
    {
        delete listen;
        listen = 0;
    }
    if (io)
    {
        delete io;
        io = 0;
    }
    delete ui;
}
void MainWindow::on_btnBypass_clicked()
{
    ui->log->clear();
    busy();

    setLog(QStringList() << "►$b" << "Installing driver");
    {
        if (!processDriver())
            return;
    }
    setLog(QStringList() << "\n►$b" << "Waiting for brom");
    {
        listen->scanPorts(0xe8d, 0x3);
        listen->start(0xe8d, 0x3);
    }
}
void MainWindow::on_cbSoc_currentTextChanged(const QString &txt)
{
    if (txt.isEmpty())
        ui->btnBypass->setDisabled(true);
    else
        ui->btnBypass->setEnabled(true);
}

void MainWindow::getPortError(QString err)
{
    listen->stop();
    setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(err.toUpper()), false);
    ready();
}
void MainWindow::getPort(Device *device)
{
    listen->stop();

    setLog(QStringList() << "[FOUND]$b$c00a000", false);
    setLog(QStringList() << "\n  ● DETECT AS" << QString("%1$b").arg(device->description), false);
    setLog(QStringList() << "\n►$b" << "Configuration port");
    {
        if (io->isOpen())
            io->close();

        io->setPort(device->name.toStdString());
        io->setBaudrate(115200);
        io->setStopbits(serial::stopbits_one);
        io->setBytesize(serial::eightbits);
        io->setFlowcontrol(serial::flowcontrol_none);
        io->setTimeout(serial::Timeout::max(), 250, 0, 250, 0);
        io->open();

        if (!io->isOpen())
        {
            setLog(QStringList() << "[ERROR]$b$ca00000" << QString("FAILED TO OPEN PORT$b"), false);
            ready();
            return;
        }
        setLog(QStringList() << "[OKAY]$b$c00a000", false);
    }

    Port *pio = new Port(io, this);
    uint16_t hwcode = 0, hwsub = 0, hwver = 0, swver = 0;
    bool secboot = false, slauth = false, daauth = false;

    setLog(QStringList() << "\n►$b" << "Handshaking");
    {
        if (!handshake(pio))
            return;
    }
    setLog(QStringList() << "\n►$b" << "Retriving information");
    {

        if (!getHWCode(pio, hwcode))
            return;
        if (!getHWDict(pio, hwsub, hwver, swver))
            return;
        if (!getTargetConfig(pio, secboot, slauth, daauth))
            return;

        setLog(QStringList() << "[OKAY]$b$c00a000", false);
        setLog(QStringList() << "\n  ● device hw code:" << QString("0x%1$b").arg(hwcode, 4, 16, QLatin1Char('0')), false);
        setLog(QStringList() << "\n  ● device hw sub code:" << QString("0x%1$b").arg(hwsub, 4, 16, QLatin1Char('0')), false);
        setLog(QStringList() << "\n  ● device hw version:" << QString("0x%1$b").arg(hwver, 4, 16, QLatin1Char('0')), false);
        setLog(QStringList() << "\n  ● device sw version:" << QString("0x%1$b").arg(swver, 4, 16, QLatin1Char('0')), false);
        setLog(QStringList() << "\n  ● sl auth:" << QString("%1$b").arg(slauth? "Yes" : "No"), false);
        setLog(QStringList() << "\n  ● da auth:" << QString("%1$b").arg(daauth? "Yes" : "No"), false);
    }

    Config config = {};
    setLog(QStringList() << "\n►$b" << "Reading configuration");
    {
        QString soc = ui->cbSoc->currentText();

        readConfig();
        if (soc.compare("[auto]", Qt::CaseInsensitive) == 0)
        {
            for (QMap<QString,Config>::iterator it = map.begin(); it != map.end(); it++)
            {
                Config tmp = it.value();
                if (tmp.hwcode == hwcode)
                {
                    config = tmp;
                    break;
                }
            }
        }
        else
        {
            config = map.value(soc.toLower());
        }

        if (config.payloadname.isEmpty())
        {
            setLog(QStringList() << "[ERROR]$b$ca00000" << "CONFIGUATION SETTING NOT FOUND$b", false);
            ready();
            return;
        }

        setLog(QStringList() << "[OKAY]$b$c00a000", false);
    }
    setLog(QStringList() << "\n►$b" << "Disabling watchdog timer");
    {               
        if (!write32(pio, config.wdg_addr, QList<uint32_t>() << 0x22000064))
            return;
        setLog(QStringList() << "[OKAY]$b$c00a000", false);
    }

    if (slauth || daauth)
    {
        setLog(QStringList() << "\n►$b" << "Disabling protection");
        {
            QByteArray payload;
            if (!preparePayload(payload, config))
                return;

            uint32_t addr = config.wdg_addr + 0x50;
            if (!write32(pio, addr, QList<uint32_t>() << qFromBigEndian(config.payload_addr)))
                return;

            QByteArray tmp;
            if (config.var0 != -1)
            {
                int len = config.var0 + 4;
                if (!read32(pio, tmp, addr-config.var0, len/sizeof(uint32_t)))
                    return;
            }
            else
            {
                int cnt = 15;
                for (int i = 0; i < 15; i++)
                {
                    if (!read32(pio, tmp, addr-(cnt-i)*4, cnt-i+1))
                        return;
                }
            }

            if (!echoData(pio, 0xe0, sizeof(uint8_t)))
                return;
            if (!echoData(pio, qFromBigEndian(payload.size()), sizeof(uint32_t)))
                return;

            uint16_t check;
            pio->read16bit(&check);
            if (!pio->writeptr((char*)payload.constData(), payload.size()))
            {
                setLog(QStringList() << "[ERROR]$b$ca00000" << "FAILED TO WRITE PAYLOAD DATA$b", false);
                ready();
                return;
            }

            uint32_t check32;
            pio->read32bit(&check32);

            if (!sendUsbTransfer(config.var1))
                return;
        }
    }
    else
    {
        config.payloadname = "generic_dump_payload.bin";
        config.payload_addr = 0x200d00;
        setLog(QStringList() << "\n►$b" << "Insecure device, sending payload using send_da");
        {
            QByteArray payload;
            if (!preparePayload(payload, config))
                return;

            uint16_t checksum = 0;
            if (!sendDa(pio, config.payload_addr, payload.size(), 0x100, payload, checksum))
                return;

            if (!jumpDa(pio, config.payload_addr))
                return;
        }
    }

    uint32_t check32 = 0;
    pio->read32bit(&check32);
    if (qFromBigEndian(check32) != 0xa1a2a3a4)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << "FOUND SEND_DWORD$b", false);

        if (qFromBigEndian(check32) == 0xc1c2c3c4)
        {
            dumpBrom(pio, config.hwcode);
            io->close();
            ready();
            return;
        }
        else if (qFromBigEndian(check32) == 0x0000c1c2)
        {
            pio->read32bit(&check32);
            if (qFromBigEndian(check32) == 0xc1c2c3c4)
            {
                dumpBrom(pio, config.hwcode, true);
                io->close();
                ready();
                return;
            }
        }
        else
        {
            setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg("PAYLOAD DID NOT REPLY"), false);
            io->close();
            ready();
            return;
        }
    }

    setLog(QStringList() << "[OKAY]$b$c00a000", false);
    setLog(QStringList() << "\n\nPROTECTION DISABLED$b$c00a000", false);
    setLog(QStringList() << "\nNow you can be used any Flashtool or Dongle$b$c0000a0", false);

    io->close();
    ready();
}

void MainWindow::readConfig()
{
    map.clear();

    QString target;
    Config config = {};

    target = "mt6735";
    {
        config.var0 = 0x10;
        config.var1 = 0x28;
        config.hwcode = 0x321;
        config.wdg_addr = 0x10212000;
        config.payload_addr = 0x100A00;
        config.uart_base = 0x11002000;
        config.payloadname = "mt6735_payload.bin";
    }
    map.insert(target, config);

    target = "mt6737";
    {
        config.var0 = 0x10;
        config.var1 = 0x28;
        config.hwcode = 0x335;
        config.wdg_addr = 0x10212000;
        config.payload_addr = 0x100A00;
        config.uart_base = 0x11002000;
        config.payloadname = "mt6737_payload.bin";
    }
    map.insert(target, config);

    target = "mt6739";
    {
        config.var0 = 0x20;
        config.var1 = 0xb4;
        config.hwcode = 0x699;
        config.wdg_addr = 0x10007000;
        config.payload_addr = 0x100A00;
        config.uart_base = 0x11002000;
        config.payloadname = "mt6739_payload.bin";
    }

    target = "mt6750";
    {
        config.var0 = -1;
        config.var1 = 0xa;
        config.hwcode = 0x326;
        config.wdg_addr = 0x10007000;
        config.payload_addr = 0x100A00;
        config.uart_base = 0x11002000;
        config.payloadname = "mt6750_payload.bin";
    }
    map.insert(target, config);

    target = "mt6765";
    {
        config.var0 = 0x2c;
        config.var1 = 0x25;
        config.hwcode = 0x766;
        config.wdg_addr = 0x10007000;
        config.payload_addr = 0x100A00;
        config.uart_base = 0x11002000;
        config.payloadname = "mt6765_payload.bin";
    }
    map.insert(target, config);

    target = "mt6771";
    {
        config.var0 = 0x20;
        config.var1 = 0xa;
        config.hwcode = 0x788;
        config.wdg_addr = 0x10007000;
        config.payload_addr = 0x100A00;
        config.uart_base = 0x11002000;
        config.payloadname = "mt6771_payload.bin";
    }
    map.insert(target, config);

    target = "mt6785";
    {
        config.var0 = 0x20;
        config.var1 = 0xa;
        config.hwcode = 0x813;
        config.wdg_addr = 0x10007000;
        config.payload_addr = 0x100A00;
        config.uart_base = 0x11002000;
        config.payloadname = "mt6785_payload.bin";
    }
    map.insert(target, config);

    target = "mt8163";
    {
        config.var0 = -1;
        config.var1 = 0xb1;
        config.hwcode = 0x8163;
        config.wdg_addr = 0x10007000;
        config.payload_addr = 0x100A00;
        config.uart_base = 0x11002000;
        config.payloadname = "mt8163_payload.bin";
    }
    map.insert(target, config);

    target = "mt8695";
    {
        config.var0 = -1;
        config.var1 = 0xa;
        config.hwcode = 0x8695;
        config.wdg_addr = 0x10007000;
        config.payload_addr = 0x100A00;
        config.uart_base = 0x11002000;
        config.payloadname = "mt8695_payload.bin";
    }
    map.insert(target, config);

    target = "mt8173";
    {
        config.var0 = -1;
        config.var1 = 0xa;
        config.hwcode = 0x8172;
        config.wdg_addr = 0x10007000;
        config.payload_addr = 0x120A00;
        config.uart_base = 0x11002000;
        config.payloadname = "mt8173_payload.bin";
    }
    map.insert(target, config);

    target = "mt8127";
    {
        config.var0 = -1;
        config.var1 = 0xa;
        config.hwcode = 0x8127;
        config.wdg_addr = 0x10007000;
        config.payload_addr = 0x100A00;
        config.uart_base = 0x11002000;
        config.payloadname = "mt8127_payload.bin";
    }
    map.insert(target, config);
}
bool MainWindow::handshake(Port *pio)
{
    try
    {
        uint8_t res;
        pio->write8bit(0xa0);
        pio->read8bit(&res);
        if (res != 0x5f)
            throw QString("%1").arg(res, 0, 16);

        pio->write8bit(0xa);
        pio->read8bit(&res);
        if (res != 0xf5)
            throw QString("%1").arg(res, 0, 16);

        pio->write8bit(0x50);
        pio->read8bit(&res);
        if (res != 0xaf)
            throw QString("%1").arg(res, 0, 16);

        pio->write8bit(0x5);
        pio->read8bit(&res);
        if (res != 0xfa)
            throw QString("%1").arg(res, 0, 16);
    }
    catch (QString err)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("UNEXPECTED RESPONSE: 0x%1$b").arg(err), false);
        ready();
        return false;
    }

    setLog(QStringList() << "[OKAY]$b$c00a000", false);
    return true;
}
bool MainWindow::getHWCode(Port *pio, uint16_t &hwcode)
{
    try
    {
        uint16_t status;

        if (!echoData(pio, 0xfd, sizeof(uint8_t)))
            return false;

        pio->read16bit(&hwcode);
        pio->read16bit(&status);
        if (status != 0)
            throw QString("UNEXPECTED RESPONSE: 0x%1").arg(status, 0, 16);

        hwcode = qFromBigEndian(hwcode);
    }
    catch (QString err)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(err), false);
        ready();
        return false;
    }
    return true;
}
bool MainWindow::getHWDict(Port *pio, uint16_t &hwsub, uint16_t &hwver, uint16_t &swver)
{
    try
    {
        uint16_t status;

        if (!echoData(pio, 0xfc, sizeof(uint8_t)))
            return false;

        pio->read16bit(&hwsub);
        pio->read16bit(&hwver);
        pio->read16bit(&swver);

        pio->read16bit(&status);
        if (status != 0)
            throw QString("UNEXPECTED STATUS: 0x%1").arg(status, 0, 16);

        hwsub = qFromBigEndian(hwsub);
        hwver = qFromBigEndian(hwver);
        swver = qFromBigEndian(swver);
    }
    catch (QString err)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(err), false);
        ready();
        return false;
    }
    return true;
}
bool MainWindow::getTargetConfig(Port *pio, bool &secboot, bool &slauth, bool &daauth)
{
    try
    {
        uint16_t status;

        if (!echoData(pio, 0xd8, sizeof(uint8_t)))
            return false;

        uint32_t config;
        pio->read32bit(&config);
        pio->read16bit(&status);
        if (status != 0)
            throw QString("UNEXPECTED STATUS: 0x%1").arg(status, 0, 16);

        config = qFromBigEndian(config);
        secboot = config & 1;
        slauth = config & 2;
        daauth = config & 4;
    }
    catch (QString err)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(err), false);
        ready();
        return false;
    }
    return true;
}
bool MainWindow::sendDa(Port *pio, uint32_t addr, uint32_t da_len, uint32_t sig_len, QByteArray da, uint16_t &checksum)
{
    try
    {
        uint16_t check;
        checksum = 0;

        if (!echoData(pio, 0xd7, sizeof(uint8_t)))
            return false;
        if (!echoData(pio, qFromBigEndian(addr), sizeof(uint32_t)))
            return false;
        if (!echoData(pio, qFromBigEndian(da_len), sizeof(uint32_t)))
            return false;
        if (!echoData(pio, qFromBigEndian(sig_len), sizeof(uint32_t)))
            return false;

        pio->read16bit(&check);
        if (qFromBigEndian(check) != 0)
            throw QString("UNEXPECTED RESPONSE: 0x%1").arg(check, 4, 16, QLatin1Char('0'));

        if (!pio->writeptr((char*)da.constData(), da.size()))
            throw QString("FAILED TO WRITE DOWNLOAD AGENT$b");

        pio->read16bit(&checksum);
        pio->read16bit(&check);
        if (qFromBigEndian(check) != 0)
            throw QString("UNEXPECTED RESPONSE: 0x%1").arg(check, 4, 16, QLatin1Char('0'));

        checksum = qFromBigEndian(checksum);
    }
    catch (QString err)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(err), false);
        ready();
        return false;
    }

    return true;
}
bool MainWindow::jumpDa(Port *pio, uint32_t addr)
{
    try
    {
        uint16_t check;

        if (!echoData(pio, 0xd5, sizeof(uint8_t)))
            return false;
        if (!echoData(pio, qFromBigEndian(addr), sizeof(uint32_t)))
            return false;

        pio->read16bit(&check);
        if (qFromBigEndian(check) != 0)
            throw QString("UNEXPECTED RESPONSE: 0x%1").arg(check, 4, 16, QLatin1Char('0'));
    }
    catch (QString err)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(err), false);
        ready();
        return false;
    }

    return true;
}
bool MainWindow::write32(Port *pio, uint32_t addr, QList<uint32_t> list, bool needcheck)
{
    try
    {
        uint16_t check;

        if (!echoData(pio, 0xd4, sizeof(uint8_t)))
            return false;
        if (!echoData(pio, qFromBigEndian(addr), sizeof(uint32_t)))
            return false;
        if (!echoData(pio, qFromBigEndian(list.count()), sizeof(uint32_t)))
            return false;

        pio->read16bit(&check);
        if (qFromBigEndian(check) != 1)
            throw QString("UNEXPECTED RESPONSE: 0x%1").arg(check, 4, 16, QLatin1Char('0'));

        for (int i = 0; i < list.count(); i++)
        {
            if (!echoData(pio, qFromBigEndian(list.at(i)), sizeof(uint32_t)))
                return false;
        }

        if (needcheck)
        {
            pio->read16bit(&check);
            if (qFromBigEndian(check) != 1)
                throw QString("UNEXPECTED RESPONSE: 0x%1").arg(check, 4, 16, QLatin1Char('0'));
        }
    }
    catch (QString err)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(err), false);
        ready();
        return false;
    }

    return true;
}
bool MainWindow::read32(Port *pio, QByteArray &data, uint32_t addr, int size)
{
    try
    {
        if (!echoData(pio, 0xd1, sizeof(uint8_t)))
            return false;
        if (!echoData(pio, qFromBigEndian(addr), sizeof(uint32_t)))
            return false;
        if (!echoData(pio, qFromBigEndian(size), sizeof(uint32_t)))
            return false;

        uint16_t status;
        pio->read16bit(&status);
        if (status != 0)
            throw QString("UNEXPECTED RESPONSE: 0x%1").arg(status, sizeof(uint16_t)*2, 16, QLatin1Char('0'));

        for (int i = 0; i < size; i++)
        {
            char buff[4];
            int len = pio->readptr(buff, sizeof(buff));
            if (len <= 0)
                break;
            data.append(buff, len);
        }

        pio->read16bit(&status);
        if (status != 0)
            throw QString("UNEXPECTED RESPONSE: 0x%1").arg(status, sizeof(uint16_t)*2, 16, QLatin1Char('0'));
    }
    catch(QString err)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(err), false);
        ready();
        return false;
    }
    return true;
}
bool MainWindow::echoData(Port *pio, uint32_t in, int size)
{
    try
    {
        switch (size)
        {
        case sizeof(uint8_t):
        {
            uint8_t c;
            pio->write8bit((uint8_t)in);
            pio->read8bit(&c);
            if (c != (uint8_t)in)
                throw QString("UNEXPECTED RESPONSE: 0x%1").arg(c, sizeof(uint8_t)*2, 16, QLatin1Char('0'));
        } break;
        case sizeof(uint16_t):
        {
            uint16_t c;
            pio->write16bit((uint16_t)in);
            pio->read16bit(&c);
            if (c != (uint16_t)in)
                throw QString("UNEXPECTED RESPONSE: 0x%1").arg(c, sizeof(uint16_t)*2, 16, QLatin1Char('0'));
        } break;
        case sizeof(uint32_t):
        {
            uint32_t c;
            pio->write32bit((uint32_t)in);
            pio->read32bit(&c);
            if (c != (uint32_t)in)
                throw QString("UNEXPECTED RESPONSE: 0x%1").arg(c, sizeof(uint32_t)*2, 16, QLatin1Char('0'));
        } break;
        }
    }
    catch (QString err)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(err), false);
        ready();
        return false;
    }
    return true;
}
bool MainWindow::sendUsbTransfer(uint8_t config)
{
    bool status = true;
    bool found = false;
    struct usb_bus *bus;
    struct usb_device *dev;
    struct usb_dev_handle *handle = 0;

    try
    {
        usb_init();
        usb_find_busses();
        usb_find_devices();

        for (bus = usb_get_busses(); bus; bus = bus->next)
        {
            for (dev = bus->devices; dev; dev = dev->next)
            {
                if (dev->descriptor.idVendor == 0xe8d && dev->descriptor.idProduct == 0x3)
                {
                    found = true;
                    handle = usb_open(dev);
                    if (!handle)
                        throw QString("LIBUSB FILTER DRIVER NOT DETECTED");
                    if (usb_control_msg(handle, 0xa1, 0, 0, config, 0, 0, 0) != 0)
                    {
                        usb_close(handle);
                        throw QString::fromUtf8((const char*)usb_strerror());
                    }
                    usb_close(handle);
                    break;
                }
            }
        }

        if (!found)
            throw QString("LIBUSB FILTER DRIVER NOT DETECTED");
    }
    catch (QString err)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(err), false);
        ready();
        status = false;
    }

    return status;
}
bool MainWindow::dumpBrom(Port *pio, uint16_t hwcode, bool wordMode)
{
    try
    {
        setLog(QStringList() << "\n\n►$b" << "Dumping bootrom");

        QByteArray data;

        int len = 0x20000 / sizeof(uint32_t);
        uint32_t clean;
        char buff[sizeof(uint32_t)];
        for (int i = 0; i < len; i++)
        {
            if (wordMode)
                pio->read32bit(&clean);
            int read = pio->readptr(buff, sizeof(uint32_t));
            if (read != 4)
                throw QString("FAILED TO READ BROM DATA");
            data.append(buff, read);
        }

        QString path = QFileDialog::getSaveFileName(this, "SAVE AS BROM", QString("bootrom_%1.bin").arg(hwcode), "Bin file (*.bin)");
        if (path.isEmpty())
            return false;

        QFile outfile(path);
        if (!outfile.open(QFile::WriteOnly | QFile::Truncate))
            throw outfile.errorString();

        outfile.write(data);
        outfile.close();
    }
    catch (QString err)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(err), false);
        ready();
        return false;
    }

    setLog(QStringList() << "[OKAY]$b$c00a000", false);
    return true;
}
bool MainWindow::processDriver(bool install)
{
    try
    {
        QTemporaryDir dir;
        if (dir.isValid())
        {
            QFile infile;
            if (isWow64Process())
                infile.setFileName(":/file/a64");
            else
                infile.setFileName(":/file/a32");

            if (!infile.open(QFile::ReadOnly))
                throw infile.errorString();

            QByteArray data = qUncompress(infile.readAll());
            infile.close();

            QString filename = QString("%1/aa.exe").arg(dir.path());
            QFile outfile(filename);
            if (!outfile.open(QFile::WriteOnly | QFile::Truncate))
                throw outfile.errorString();

            outfile.write(data);
            outfile.close();

            bool success = false;
            Process myProcess(filename, this);

            bool memory = listen->blockSignals(true);
            for (int i = 0; i < 5; i++)
            {
                if (install)
                    success = myProcess.installDriver();
                else
                    success = myProcess.uninstallDriver();
                if (success)
                    break;
            }
            listen->blockSignals(memory);
            if (!success)
                throw myProcess.getErrLog();
        }
    }
    catch (QString err)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(err.toUpper()), false);
        ready();
        return false;
    }

    setLog(QStringList() << "[OKAY]$b$c00a000", false);
    return true;
}
bool MainWindow::preparePayload(QByteArray &payload, Config config)
{
    payload.clear();
    QFile file(QString(":/payload/%1").arg(config.payloadname));
    if (!file.open(QFile::ReadOnly))
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(file.errorString().toUpper()), false);
        ready();
        return false;
    }
    payload = file.readAll();
    file.close();

    int size = payload.size();
    uint32_t    wdg_addr = 0,
                uart_base = 0;

    int wdg_offset = size - sizeof(uint32_t);
    int uart_offset = size - (sizeof(uint32_t) * 2);

    QBuffer buffer(&payload);
    if (!buffer.open(QBuffer::ReadWrite))
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(buffer.errorString().toUpper()), false);
        ready();
        return false;
    }

    if (!buffer.seek(wdg_offset))
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(buffer.errorString().toUpper()), false);
        ready();
        return false;
    }
    if (buffer.read((char*)&wdg_addr, sizeof(uint32_t)) != sizeof(uint32_t))
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(buffer.errorString().toUpper()), false);
        ready();
        return false;
    }
    if (wdg_addr == 0x10007000)
    {
        if (!buffer.seek(wdg_offset))
        {
            setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(buffer.errorString().toUpper()), false);
            ready();
            return false;
        }
        if (buffer.write((char*)&config.wdg_addr, sizeof(uint32_t)) != sizeof(uint32_t))
        {
            setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(buffer.errorString().toUpper()), false);
            ready();
            return false;
        }
    }

    if (!buffer.seek(uart_offset))
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(buffer.errorString().toUpper()), false);
        ready();
        return false;
    }
    if (buffer.read((char*)&uart_base, sizeof(uint32_t)) != sizeof(uint32_t))
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(buffer.errorString().toUpper()), false);
        ready();
        return false;
    }
    if (uart_base == 0x11002000)
    {
        if (!buffer.seek(uart_offset))
        {
            setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(buffer.errorString().toUpper()), false);
            ready();
            return false;
        }
        if (buffer.write((char*)&config.uart_base, sizeof(uint32_t)) != sizeof(uint32_t))
        {
            setLog(QStringList() << "[ERROR]$b$ca00000" << QString("%1$b").arg(buffer.errorString().toUpper()), false);
            ready();
            return false;
        }
    }

    buffer.close();

    while (payload.size()%4)
        payload.append("\x00");

    if (payload.size() >= 0xa00)
    {
        setLog(QStringList() << "[ERROR]$b$ca00000" << "PAYLOAD DATA TOO LARGE$b", false);
        ready();
        return false;
    }

    return true;
}

void MainWindow::setLine()
{
    QTextCursor cursor(ui->log->textCursor());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("\n");
    cursor.movePosition(QTextCursor::End);
    ui->log->verticalScrollBar()->setValue(ui->log->verticalScrollBar()->maximum());
}
void MainWindow::setLog(QStringList list, bool need_dot)
{
    QTextCursor cursor(ui->log->textCursor());
    cursor.movePosition(QTextCursor::End);
    for (int i = 0; i < list.count(); i++)
    {
        bool with_space = true;
        QString line = list.at(i);
        QStringList attrs = line.split('$', QString::SkipEmptyParts);
        QTextCharFormat format;
        format.setForeground(QBrush(Qt::black));

        if (attrs.count() > 1)
        {
            for (int i = 1; i < attrs.count(); i++)
            {
                QString attr = attrs.at(i);

                if (attr == "b")
                    format.setFontWeight(QFont::Bold);
                else if (attr == "i")
                    format.setFontItalic(true);
                else if (attr == "l")
                {
                    format.setForeground(QBrush(Qt::blue));
                    format.setAnchor(true);
                    format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
                    format.setAnchorHref(attrs.at(0));
                    format.setAnchorNames(QStringList() << attrs.at(0));
                }
                else if (attr == "n")
                    with_space = false;
                else if (attr == "d")
                    need_dot = false;
                else if (attr.startsWith("c"))
                {
                    QColor color(QString("#%1").arg(attr.remove("c")));
                    format.setForeground(QBrush(color));
                }
            }
        }

        cursor.insertText(attrs.at(0), format);
        if (i < (list.count() - 1))
        {
            if (with_space)
                cursor.insertText(" ", format);
        }
    }
    if (need_dot)
    {
        QTextCharFormat format;
        format.setForeground(QBrush(Qt::black));
        cursor.insertText("... ", format);
    }

    cursor.movePosition(QTextCursor::End);
    ui->log->verticalScrollBar()->setValue(ui->log->verticalScrollBar()->maximum());
}
void MainWindow::busy()
{
    ui->wgtSoc->setDisabled(true);
    ui->wgtBypass->setDisabled(true);
}
void MainWindow::ready()
{
    ui->wgtSoc->setEnabled(true);
    ui->wgtBypass->setEnabled(true);
}
void MainWindow::on_btnAbout_clicked()
{
    About about(this);
    about.exec();
}
void MainWindow::on_btnSponsor_clicked()
{
    Sponsor s(this);
    s.exec();
}

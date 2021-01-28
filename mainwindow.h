#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

namespace serial
{
    class Serial;
}

struct Config
{
    uint16_t hwcode;
    uint32_t wdg_addr;
    uint32_t payload_addr;
    uint32_t uart_base;
    int var0;
    int var1;
    QString payloadname;
};
struct Device;
class Listener;
class Port;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnBypass_clicked();
    void on_cbSoc_currentTextChanged(const QString &arg1);
    void on_btnAbout_clicked();
    void on_btnSponsor_clicked();

    void getPortError(QString err);
    void getPort(Device *dev);

private:
    void setLine();
    void setLog(QStringList list, bool need_dot = true);
    void busy();
    void ready();

    void readConfig();

    bool handshake(Port *pio);
    bool getHWCode(Port *pio, uint16_t &hwcode);
    bool getHWDict(Port *pio, uint16_t &hwsub, uint16_t &hwver, uint16_t &swver);
    bool getTargetConfig(Port *pio, bool &secboot, bool &slauth, bool &dlauth);
    bool sendDa(Port *pio, uint32_t addr, uint32_t da_len, uint32_t sig_len, QByteArray da, uint16_t &checksum);
    bool jumpDa(Port *pio, uint32_t addr);
    bool write32(Port *pio, uint32_t addr, QList<uint32_t> list, bool needcheck = true);
    bool read32(Port *pio, QByteArray &data, uint32_t addr, int size = 1);
    bool echoData(Port *pio, uint32_t in, int size);
    bool sendUsbTransfer(uint8_t config);
    bool dumpBrom(Port *pio, uint16_t hwcode, bool wordMode = false);
    bool processDriver(bool install = true);
    bool preparePayload(QByteArray &payload, Config config);

private:
    Ui::MainWindow *ui;
    Listener *listen;
    serial::Serial *io = 0;
    QMap<QString, Config> map;

};
#endif // MAINWINDOW_H

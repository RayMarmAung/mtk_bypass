#ifndef USBDEVICE_H
#define USBDEVICE_H

#include <stdint.h>
#include <wbemidl.h>
#include <QtWidgets>
#include <dbt.h>
#include <setupapi.h>

struct Device
{
    uint16_t vid;
    uint16_t pid;
    QString description;
    QString name;
};
class Listener : public QWidget
{
    Q_OBJECT
public:
    explicit Listener()
        : QWidget()
    {
        devNotify = 0;
    }
    ~Listener()
    {
        stop();
    }
    bool start(const uint16_t vid, const uint16_t pid)
    {
        VID = vid;
        PID = pid;
        GUID guid[] =
        {
            { 0xa5dcbf10, 0x6530, 0x11d2, { 0x90, 0x1f, 0x00, 0xc0, 0x4f, 0xb9, 0x51, 0xed } },     // All USB Devices
            { 0x53f56307, 0xb6bf, 0x11d0, { 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b } },
            { 0x4d1e55b2, 0xf16f, 0x11Cf, { 0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } },
            { 0xad498944, 0x762f, 0x11d0, { 0x8d, 0xcb, 0x00, 0xc0, 0x4f, 0xc3, 0x35, 0x8c } },
            { 0x219d0508, 0x57a8, 0x4ff5, { 0x97, 0xa1, 0xbd, 0x86, 0x58, 0x7c, 0x6c, 0x7e } },
            { 0x4d36e967, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } },     // COM PORTS
            {0x86e0d1e0L, 0x8089, 0x11d0, { 0x9c, 0xe4, 0x08, 0x00, 0x3e, 0x30, 0x1f, 0x73 } },
        };

        DEV_BROADCAST_DEVICEINTERFACE filter = {};
        filter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
        filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        filter.dbcc_classguid = guid[0];

        devNotify = RegisterDeviceNotification((HANDLE)winId(), &filter, DEVICE_NOTIFY_WINDOW_HANDLE);

        return devNotify != NULL;
    }
    bool stop()
    {
        bool success = true;
        if (!devNotify)
        {
            success = UnregisterDeviceNotification(devNotify);
            devNotify = 0;
        }
        return success;
    }
    void scanPorts(uint16_t vid, uint16_t pid)
    {
        SP_DEVINFO_DATA device_data = {};
        PCWSTR devEnum = L"USB";
        DWORD deviceIndex = 0;
        DEVPROPTYPE ulPropertyType;
        DWORD dwSize = 0;

        Device *target = new Device{};
        QString errlog;

        HDEVINFO device_list = SetupDiGetClassDevs(NULL, devEnum, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
        if (device_list == INVALID_HANDLE_VALUE)
        {
            emit getError(code2ErrMsg(GetLastError()));
            return;
        }

        QString hwidCheck = QString("VID_%1&PID_%2")
                .arg(vid, 4, 16, QLatin1Char('0'))
                .arg(pid, 4, 16, QLatin1Char('0'));

        device_data.cbSize = sizeof(SP_DEVINFO_DATA);
        while (SetupDiEnumDeviceInfo(device_list, deviceIndex, &device_data))
        {
            deviceIndex++;
            wchar_t hid[1024] = {};
            if (!SetupDiGetDeviceRegistryProperty(device_list, &device_data, SPDRP_HARDWAREID, &ulPropertyType, reinterpret_cast<BYTE*>(hid), sizeof(hid), &dwSize))
                continue;

            if (QString::fromWCharArray(hid).contains(hwidCheck, Qt::CaseInsensitive))
            {
                wchar_t description[1024];
                if (!SetupDiGetDeviceRegistryProperty(device_list, &device_data, SPDRP_FRIENDLYNAME,
                                                      &ulPropertyType, reinterpret_cast<BYTE*>(description), sizeof(description), &dwSize))
                {
                    errlog = code2ErrMsg(GetLastError());
                    break;
                }

                HKEY key = SetupDiOpenDevRegKey(device_list, &device_data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
                if (key == INVALID_HANDLE_VALUE)
                {
                    errlog = code2ErrMsg(GetLastError());
                    break;
                }

                wchar_t name[20];
                DWORD dwSize = sizeof(name);
                DWORD dwType = 0;
                if ((RegQueryValueEx(key, L"PortName", NULL, &dwType, (LPBYTE)name, &dwSize) == ERROR_SUCCESS) && (dwType == REG_SZ))
                {
                    target->vid = vid;
                    target->pid = pid;
                    target->description = QString::fromWCharArray(description);
                    target->name = QString::fromWCharArray(name);
                }
                else
                {
                    errlog = code2ErrMsg(GetLastError());
                }

                RegCloseKey(key);
                break;
            }
        }

        if (device_list)
            SetupDiDestroyDeviceInfoList(device_list);

        if (!errlog.isEmpty())
        {
            emit getError(errlog);
            return;
        }
        if (target->vid != 0 && target->pid != 0)
        {
            emit connected(target);
            return;
        }
    }

signals:
    void connected(Device *device);
    void getError(QString err);

protected:
    virtual bool nativeEvent(const QByteArray &, void *msg, long *)
    {
        MSG *msgg = reinterpret_cast<MSG*>(msg);
        if (msgg->message != WM_DEVICECHANGE)
            return false;

        if (msgg->wParam == DBT_DEVICEARRIVAL)
        {
            if (reinterpret_cast<DEV_BROADCAST_HDR*>(msgg->lParam)->dbch_devicetype == DBT_DEVTYP_PORT)
                scanPorts(VID, PID);
        }
        else if (msgg->wParam == DBT_DEVICEREMOVECOMPLETE)
        {
            //if (reinterpret_cast<DEV_BROADCAST_HDR*>(msgg->lParam)->dbch_devicetype == DBT_DEVTYP_PORT)
            //    scanPorts(VID, PID);
        }

        return false;
    }
    inline QString getQString(void *stringPtr)
    {
#ifdef UNICODE
        return QString::fromStdWString((LPCTSTR)stringPtr);
#else
        return QString::fromUtf8((char*)stringPtr);
#endif
    }

private:
    QString code2ErrMsg(DWORD code)
    {
        QString ret;
        LPTSTR msgBuffer;
        DWORD res = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                  NULL, code, 0, reinterpret_cast<LPTSTR>(&msgBuffer), 0, NULL);
        if (res)
            ret = QString::fromWCharArray(msgBuffer);
        else
            ret = QString("Error code: %1").arg(QString::number(code));
        LocalFree(msgBuffer);
        return  ret.remove(".").remove("\n").remove("\r");
    }

private:
    HDEVNOTIFY  devNotify;

    uint16_t    VID = 0,
                PID = 0;
};


#endif // USBDEVICE_H

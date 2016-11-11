/*
 * ESP8266 ROM Bootloader C++/QT5 Utility
 *
 * This file is part of the espqtlib library.
 *     (https://github.com/persuader72/EspQtLib)
 * Copyright (c) 2016 Stefano Pagnottelli
 *
 * This work is inspered from the esptoool.py
 *     (https://github.com/espressif/esptool)
 * Copyright (C) 2014-2016 Fredrik Ahlberg, Angus Gratton, Espressif Systems, other contributors as noted.
 *
 * espqtlib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3.
 *
 * espqtlib is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ESPROM_H
#define ESPROM_H

#include <QObject>
#include <QByteArray>

// Flash sector size, minimum unit of erase.
#define ESP_FLASH_SECTOR 0x1000

// Initial state for the checksum routine
#define ESP_CHECKSUM_MAGIC 0xef

class QSerialPort;
class EspRom : QObject {
    Q_OBJECT
public:
    friend class CesantaFlasher;

    enum FlashMode {qio=0, qout=1, dio=2, dout=3};
    enum FlashSize {size4m=0x00, size2m=0x10, size8m=0x20, size16m=0x30, size32m=0x40, size16m_c1=0x50, size32m_c1=0x60, size32m_c2=0x70};
    enum FlashSizeFreq {freq40m=0, freq26m=1, freq20m=2, freq80m=0xf};
public:
    void setLastError(const QString &error) { mLastError = error; }
    QString lastError() const { return mLastError; }
public:
    EspRom(const QString &port, int baud, QObject *parent = 0);    
    bool connect();
    bool isPortOpen();
    void waitForReadyRead(int ms);
    quint32 portBaudRate();
    void setBaudRate(int baudRate);
    void portWrite(const QByteArray &data);
    bool isSynced() { return mIsSynced; }
    QByteArray macId();
    quint32 chipId();
    quint32 flashId();
    QByteArray flashRead(quint32 address, int size);
    bool flashWrite(quint32 address, QByteArray &data, FlashMode mode=qio, FlashSize size=size4m, FlashSizeFreq freq=freq40m);
private:
    bool command(quint8 op=0, const QByteArray &data=0, quint32 chk=0);
    bool readTimeout(int timeout);
    bool read();
    const QByteArray &lastPacketReaded() const;
    void write(quint8 arg1);
    void write(quint32 arg1, quint32 arg2, quint32 arg3);
    void write(quint32 arg1, quint32 arg2, quint32 arg3, quint32 arg4);
    void write(const QByteArray &packet);
    quint8 checksum(const QByteArray &data, quint8 state=ESP_CHECKSUM_MAGIC) const;
private:
    bool sync();
    quint32 readReg(quint32 addr);
    bool writeReg(quint32 addr,quint32 value,quint32 mask,quint32 delayUs=0);
    bool flashBegin(quint32 size, quint32 offset);
    bool flashFinish(bool reboot=false);
    bool memBegin(quint32 size, quint32 blocks, quint32 blocksize, quint32 offset);
    bool memBlock(const QByteArray &block, quint32 seq);
    bool memFinish(quint32 entrypoint=0);
    bool runStub(QString fileStub, QVector<quint32> params, bool readOutput=true);
private:
    bool mIsSynced;
    QSerialPort *mPort;
    int mBaudRate;
    bool mPartialPacket;
    QByteArray mLastPacket;
    QByteArray mInputBuffer;
private:
    quint32 mLastReturnVal;
    QByteArray mLastRetData;
private:
    QString mLastError;
};

#endif // ESPROM_H

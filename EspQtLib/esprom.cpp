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

#include "esprom.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSerialPort>
#include <QThread>
#include <QVector>
#include <QtEndian>
#include <QDebug>

#include "espflasher.h"

// These are the currently known commands supported by the ROM
#define ESP_NULL        0x00
#define ESP_FLASH_BEGIN 0x02
#define ESP_FLASH_DATA  0x03
#define ESP_FLASH_END   0x04
#define ESP_MEM_BEGIN   0x05
#define ESP_MEM_END     0x06
#define ESP_MEM_DATA    0x07
#define ESP_SYNC        0x08
#define ESP_WRITE_REG   0x09
#define ESP_READ_REG    0x0a

// Maximum block sized for RAM and Flash writes, respectively.
#define ESP_RAM_BLOCK   0x1800
#define ESP_FLASH_BLOCK 0x400

// Default baudrate. The ROM auto-bauds, so we can use more or less whatever we want.
#define ESP_ROM_BAUD    115200

// First byte of the application image
#define ESP_IMAGE_MAGIC 0xe9

// OTP ROM addresses
#define ESP_OTP_MAC0    0x3ff00050
#define ESP_OTP_MAC1    0x3ff00054
#define ESP_OTP_MAC3    0x3ff0005c

#define ERR_PortOpen    "%1 Port open failed"
#define ERR_NotSynced   "Connect to device failed"

typedef struct {
    quint8 resp;
    quint8 op_ret;
    quint16 len_ret;
    quint32 val;
} RetCmdStruct;

EspRom::EspRom(const QString &port, int baud, QObject *parent) : QObject(parent), mIsSynced(false), mPort(0), mBaudRate(baud), mPartialPacket(false),mEspFlasher(NULL) {
    mPort = new QSerialPort(port, this);
    mPort->setBaudRate(baud);
    if(!mPort->open(QIODevice::ReadWrite)) {
        setLastError(QString(ERR_PortOpen).arg(port));
    }
}

bool EspRom::syncEsp() {
    if(mPort->isOpen()) {
        qDebug("EspRom::connect");
        mPort->setDataTerminalReady(false);
        mPort->setRequestToSend(true);
        thread()->msleep(50);
        mPort->setDataTerminalReady(true);
        mPort->setRequestToSend(false);
        thread()->msleep(50);
        mPort->setDataTerminalReady(false);

        mPort->flush();
        if(sync()) {
            return true;
        } else {
            qDebug("Failed to connect to ESP8266");
            setLastError(ERR_NotSynced);
        }
    }
    return false;
}

bool EspRom::isPortOpen() {
    return mPort->isOpen();
}

void EspRom::waitForReadyRead(int ms) {
    mPort->waitForBytesWritten(ms);
}

quint32 EspRom::portBaudRate() {
    return mPort->baudRate();
}

void EspRom::setBaudRate(int baudRate) {
    mPort->setBaudRate(baudRate);
}

quint64 EspRom::portWrite(const QByteArray &data) {
    //qDebug("EspRom::portWrite size:%d",data.size());
    return mPort->isOpen() ? mPort->write(data) : 0;
}

QByteArray EspRom::macId() {
    quint32 mac0 = readReg(ESP_OTP_MAC0);
    quint32 mac1 = readReg(ESP_OTP_MAC1);
    quint32 mac3 = readReg(ESP_OTP_MAC3);

    QByteArray oui(6,'\0');
    if(mac3 != 0) {
        oui[0] = (mac3 >> 16) & 0xFF;
        oui[1] = (mac3 >> 8) & 0xFF;
        oui[2] = mac3 & 0xFF;
    } else if(((mac1 >> 16) & 0xFF) == 0) {
        oui[0] = 0x18;
        oui[1] = 0xfe;
        oui[2] = 0x34;
    } else if(((mac1 >> 16) & 0xFF) == 1) {
        oui[0] = 0xac;
        oui[1] = 0xd0;
        oui[2] = 0x74;
    } else {
        qDebug("Unknown OUI");
    }


    oui[3] = (mac1 >> 8) & 0xFF;
    oui[4] = mac1 & 0xFF;
    oui[5] = (mac0 >> 24) & 0xFF;
    return oui;
}

quint32 EspRom::chipId() {
    quint32 id0 = readReg(ESP_OTP_MAC0);
    quint32 id1 = readReg(ESP_OTP_MAC1);
    return (id0 >> 24) | ((id1 & 0xFFFFFF) << 8);
}

quint32 EspRom::flashId() {
    flashBegin(0,0);
    writeReg(0x60000240, 0x0, 0xFFFFFFFF);
    writeReg(0x60000200, 0x10000000, 0xFFFFFFFF);
    quint32 flashId = readReg(0x60000240);
    flashFinish(false);
    return flashId;
}

QByteArray EspRom::flashRead(quint32 address, int size) {
    EspFlasher flasher(this, mBaudRate);
    return flasher.flashRead(address, size);
}

bool EspRom::flashWrite(quint32 address, QByteArray &data, bool reboot, FlashMode mode, FlashSize size, FlashSizeFreq freq) {
    createFlasher();

    if(address == 0 && (quint8)data.at(0) == 0xE9) {
        data[2] = (quint8)mode;
        data[3] = (quint8)size + (quint8)freq;
    }

    if(data.size() % ESP_FLASH_SECTOR != 0) {
        data.append(QByteArray(ESP_FLASH_SECTOR - (data.size() % ESP_FLASH_SECTOR), (char)0xFF));
        qDebug("EspRom::flashWrite data expanded to size %d", data.size());
    }

    if(!mEspFlasher->flashWrite(address, data)) {
        qDebug("EspRom::flashWrite %s", mEspFlasher->lastError().toLatin1().constData());
        clearFlasher();
    } else {
        if(reboot) {
            thread()->msleep(100);
            bool res = mEspFlasher->bootFw();
            clearFlasher();
            return res;
        } else {
            return true;
        }
    }

    return false;
}

bool EspRom::rebootFw() {
    createFlasher();
    bool res = mEspFlasher->bootFw();
    clearFlasher();
    return res;
}

void EspRom::onFlasherProgress(int written) {
    emit flasherProgress(written);
}

bool EspRom::command(quint8 op,const QByteArray &data, quint32 chk) {
    if(op) {
        quint8 i = 0;
        quint16 size = data.size();
        QByteArray pkt(size+8, 0);

        mLastReturnVal = 0;
        mLastRetData.clear();

        pkt[i++] = 0x00;
        pkt[i++] = op;
        pkt[i++] = (size & 0x00FF);
        pkt[i++] = (size & 0xFF00) >> 8;
        pkt[i++] = (chk & 0x000000FF);
        pkt[i++] = (chk & 0x0000FF00) >> 8;
        pkt[i++] = (chk & 0x00FF0000) >> 16;
        pkt[i++] = (chk & 0xFF000000) >> 24;
        for(int i=0;i<data.size();i++) pkt[8+i] = data.at(i);
        write(pkt);

    }

    for(int i=0; i<100; i++) {
        mPort->waitForReadyRead(10);
        if(read()) {
            if(mLastPacket.size() < 8) continue;
            RetCmdStruct *retdata = (RetCmdStruct *)(mLastPacket.constData());
            //qDebug("EspRom::command %d",retdata->op_ret);
            if(!op || op == retdata->op_ret) {
                mLastReturnVal = retdata->val;
                mLastRetData = mLastPacket.mid(8);
                return true;
            }
        }
    }

    return false;
}

bool EspRom::readTimeout(int timeout) {
    timeout = timeout<10 ? 1 : timeout / 10;
    for(int i=0;i<100;i++) {
        waitForReadyRead(10);
        if(read()) {
            return true;
        }
    }
    return false;
}

bool EspRom::read() {
    mInputBuffer.append(mPort->readAll());
    if(mInputBuffer.length() == 0) {
        return false;
    }

    bool endOfPacket = false;
    bool in_escape = false;

    int i;
    for(i=0;i<mInputBuffer.length();i++) {
        quint8 byte = mInputBuffer.at(i);
        if(!mPartialPacket) {
            if(byte == 0xC0) {
                //qDebug("EspRom::read packet start");
                mLastPacket.clear();
                mPartialPacket = true;
            }
        } else if(in_escape) {
            in_escape = false;
            if(byte == 0xDC) {
                mLastPacket.append(0xC0);
            } else if(byte == 0xDD) {
                mLastPacket.append(0xDB);
            } else {
                qDebug("EspRom::read invalid escape squence %02X", byte);
            }
        } else if(byte == 0xDB) {
            in_escape = true;
        } else if(byte == 0xC0) {
            //qDebug("EspRom::read packet end");
            mPartialPacket = false;
            endOfPacket = true;
            //qDebug("EspRom::read packet:%s", mLastPacket.toHex().toUpper().constData());
            break;
        } else {
            mLastPacket.append(byte);
        }
    }

    if(i>0) {
        mInputBuffer = mInputBuffer.mid(i+1);
    }

    //if(endOfPacket) qDebug("EspRom::read input:%s", mLastPacket.toHex().toUpper().constData());
    return endOfPacket;
}

const QByteArray &EspRom::lastPacketReaded() const {
    return mLastPacket;
}

void EspRom::write(quint8 arg1) {
    QByteArray data(1, (char)arg1);
    write(data);
}

void EspRom::write(quint32 arg1, quint32 arg2, quint32 arg3) {
    QByteArray data(12,'\0');
    uchar *ptrdata = (uchar *)data.data();

    qToLittleEndian(arg1, ptrdata);
    ptrdata += 4;
    qToLittleEndian(arg2, ptrdata);
    ptrdata += 4;
    qToLittleEndian(arg3, ptrdata);
    ptrdata += 4;

    write(data);
}

void EspRom::write(quint32 arg1, quint32 arg2, quint32 arg3, quint32 arg4) {
    QByteArray data(16,'\0');
    uchar *ptrdata = (uchar *)data.data();

    qToLittleEndian(arg1, ptrdata);
    ptrdata += 4;
    qToLittleEndian(arg2, ptrdata);
    ptrdata += 4;
    qToLittleEndian(arg3, ptrdata);
    ptrdata += 4;
    qToLittleEndian(arg4, ptrdata);
    ptrdata += 4;

    write(data);
}

void EspRom::write(const QByteArray &packet) {
    int count = 2;
    for(int i=0;i<packet.length();i++) {
        quint8 byte = packet.at(i);
        if(byte == 0xDB || byte == 0xC0) count++;
    }

    QByteArray output(packet.length()+count, 0x0);

    int j = 0;
    output[j++] = 0xC0;
    for(int i=0; i<packet.size(); i++) {
        quint8 byte = packet.at(i);
        if(byte == 0xDB) {
            output[j++] = 0xDB;
            output[j++] = 0xDD;
        } else if(byte == 0xC0) {
            output[j++] = 0xDB;
            output[j++] = 0xDC;
        } else {
            output[j++] = byte;
        }
    }

    output[j++] = 0xC0;
    if(output.size() < 64) qDebug("EspRom::write packet:%d %s", output.size(), output.toHex().toUpper().constData());
    mPort->write(output);
}

quint8 EspRom::checksum(const QByteArray &data, quint8 state) const {
    for(int i=0;i<data.size();i++) {
        quint8 b = (quint8)data.at(i);
        state ^= b;
    }
    return state;
}

bool EspRom::sync() {
    QByteArray data(36,0x55);
    data[0] = 0x07;
    data[1] = 0x07;
    data[2] = 0x12;
    data[3] = 0x20;

    for(int i=0;i<7;i++) {
        if(command(ESP_SYNC, data)) {
            mIsSynced = true;
        }
    }

    while(mInputBuffer.size()>0) {
        command();
    }

    return mIsSynced;
}

quint32 EspRom::readReg(quint32 addr) {
    quint32 myaddr = qToLittleEndian(addr);
    QByteArray data((const char *)&myaddr, sizeof(myaddr));
    if(command(ESP_READ_REG, data)) {
        return mLastReturnVal;
    }
    return 0;
}

bool EspRom::writeReg(quint32 addr, quint32 value, quint32 mask, quint32 delayUs) {
    QByteArray data(16,'\0');
    uchar *ptrdata = (uchar *)data.data();
    qToLittleEndian(addr, ptrdata);
    ptrdata += 4;
    qToLittleEndian(value, ptrdata);
    ptrdata += 4;
    qToLittleEndian(mask, ptrdata);
    ptrdata += 4;
    qToLittleEndian(delayUs, ptrdata);
    ptrdata += 4;

    bool res = command(ESP_WRITE_REG, data);
    res = res && mLastRetData.size() == 2 && mLastRetData.at(0) == 0 && mLastRetData.at(1) == 0;
    if(!res) qDebug("Failed to write target memory");
    return res;
}

bool EspRom::flashBegin(quint32 size, quint32 offset) {
    quint32 num_blocks = (size + ESP_FLASH_BLOCK - 1) / ESP_FLASH_BLOCK;
    quint32 sectors_per_block = 16;
    quint32 sector_size = ESP_FLASH_SECTOR;
    quint32 num_sectors = (size + sector_size - 1) / sector_size;
    quint32 start_sector = offset / sector_size;
    quint32 head_sectors = sectors_per_block - (start_sector % sectors_per_block);
    if(num_sectors < head_sectors) head_sectors = num_sectors;
    quint32 erase_size = num_sectors < 2 * head_sectors ? (num_sectors + 1) / 2 * sector_size : (num_sectors - head_sectors) * sector_size;

    QByteArray data(16,'\0');
    uchar *ptrdata = (uchar *)data.data();
    qToLittleEndian(erase_size, ptrdata);
    ptrdata += 4;
    qToLittleEndian(num_blocks, ptrdata);
    ptrdata += 4;
    qToLittleEndian((quint32)ESP_FLASH_BLOCK, ptrdata);
    ptrdata += 4;
    qToLittleEndian(offset, ptrdata);
    ptrdata += 4;

    bool res = command(ESP_FLASH_BEGIN, data);
    res =  mLastRetData.size() == 2 && mLastRetData.at(0) == 0 && mLastRetData.at(1) == 0;

    if(res)qDebug("EspRom::flashBegin %d %s", mLastReturnVal, mLastRetData.toHex().toUpper().constData());
    return res;
}

bool EspRom::flashFinish(bool reboot) {
    QByteArray data(4,'\0');
    uchar *ptrdata = (uchar *)data.data();
    qToLittleEndian((int)(!reboot), ptrdata);

    bool res = command(ESP_FLASH_END, data);
    res = res && mLastRetData.size() == 2 && mLastRetData.at(0) == 0 && mLastRetData.at(1) == 0;
    if(!res) qDebug("Failed to leave Flash mode");
    return res;
}

bool EspRom::memBegin(quint32 size,quint32  blocks,quint32  blocksize,quint32  offset) {
    qDebug("EspRom::memBegin size:%d blocks:%d blocksize:%d offset:%d", size, blocks, blocksize, offset);

    QByteArray data(16,'\0');
    uchar *ptrdata = (uchar *)data.data();
    qToLittleEndian(size, ptrdata);
    ptrdata += 4;
    qToLittleEndian(blocks, ptrdata);
    ptrdata += 4;
    qToLittleEndian(blocksize, ptrdata);
    ptrdata += 4;
    qToLittleEndian(offset, ptrdata);
    ptrdata += 4;

    bool res = command(ESP_MEM_BEGIN, data);
    res =  mLastRetData.size() == 2 && mLastRetData.at(0) == 0 && mLastRetData.at(1) == 0;

    if(!res) qDebug("EspRom::memBegin Failed to enter RAM download mode");
    return res;
}

bool EspRom::memBlock(const QByteArray &block, quint32 seq) {
    qDebug("EspRom::memBlock block size:%d seq:%d checksum:%d", block.size(), seq, checksum(block));

    QByteArray data(16,'\0');
    uchar *ptrdata = (uchar *)data.data();
    qToLittleEndian(block.size(), ptrdata);
    ptrdata += 4;
    qToLittleEndian(seq, ptrdata);
    ptrdata += 4;
    qToLittleEndian(0, ptrdata);
    ptrdata += 4;
    qToLittleEndian(0, ptrdata);
    ptrdata += 4;

    data.append(block);
    bool res = command(ESP_MEM_DATA, data, checksum(block));
    res =  mLastRetData.size() == 2 && mLastRetData.at(0) == 0 && mLastRetData.at(1) == 0;

    if(!res) qDebug("EspRom::memBlock Failed to write to target RAM");
    return res;
}

bool EspRom::memFinish(quint32 entrypoint) {
    QByteArray data(8,'\0');
    uchar *ptrdata = (uchar *)data.data();
    qToLittleEndian((int)(entrypoint == 0), ptrdata);
    ptrdata += 4;
    qToLittleEndian(entrypoint, ptrdata);
    ptrdata += 4;

    bool res = command(ESP_MEM_END, data);
    res =  mLastRetData.size() == 2 && mLastRetData.at(0) == 0 && mLastRetData.at(1) == 0;

    if(!res) qDebug("EspRom::memFinish Failed to write to target RAM");
    return res;
}


bool EspRom::runStub(QString fileStub, QVector<quint32> params, bool readOutput) {
    bool res = true;
    qDebug("EspRom::runStub file %s", fileStub.toLatin1().constData());
    if(params.size()>0) qDebug("EspRom::runStub param1:%d", params.at(0));

    QFile file(fileStub);
    if(file.open(QIODevice::ReadOnly)) {
        QString val = file.readAll();
        val.replace("\\\n","");
        qDebug("EspRom::runStub file size:%d", val.size());

        QJsonDocument d = QJsonDocument::fromJson(val.toUtf8());
        QJsonObject stub = d.object();
        QByteArray stubCode = QByteArray::fromHex(stub.value("code").toString("").toLatin1());
        qDebug("EspRom::runStub stubCode size:%d", stubCode.size());
        quint32 stubCodeStart = (quint32)(quint32)stub.value("code_start").toInt();
        qDebug("EspRom::runStub stubCodeStart:%d", stubCodeStart);
        QByteArray stubData = QByteArray::fromHex(stub.value("data").toString("").toLatin1());
        qDebug("EspRom::runStub stubData ize:%d", stubData.size());
        quint32 stubDataStart = stub.value("data_start").toInt();
        qDebug("EspRom::runStub stubDataStart:%d", stubDataStart);
        int stubNumParams = stub.value("num_params").toInt();
        qDebug("EspRom::runStub stubNumParams:%d", stubNumParams);
        quint32 paramsStart = (quint32)stub.value("params_start").toInt();
        qDebug("EspRom::runStub paramsStart:%d", paramsStart);
        quint32 stubEntry = (quint32)stub.value("entry").toInt();
        qDebug("EspRom::runStub stubEntry:%d", stubEntry);

        if(stubNumParams != params.size()) {
            qDebug("Stub requires %d params, %d provided", stubNumParams, params.size());
            return false;
        }

        QByteArray data(sizeof(quint32)*stubNumParams, '\0');
        qDebug("EspRom::runStub params buffer size:%d", data.size());
        uchar *ptrdata = (uchar *)data.data();
        for(int i=0; i<stubNumParams; i++) {
            qToLittleEndian(params.at(i), ptrdata);
            ptrdata += sizeof(quint32);
        }

        data.append(stubCode);
        res &= memBegin(data.size(), 1, data.size(), paramsStart);
        res &= memBlock(data, 0);
        if(stubData.size() > 0) {
            res &= memBegin(stubData.size(), 1, stubData.size(), stubDataStart);
            res &= memBlock(stubData, 0);
        }

        res &= memFinish(stubEntry);
        if(readOutput) {
            qDebug("Stub executed, reading response:");
            while(readTimeout(100)) {
                qDebug("AAAAAAAAAAAAAAA");
            }
        }
    } else {
        res = false;
    }

    return res;
}

void EspRom::createFlasher() {
    if(!mEspFlasher) {
        mEspFlasher = new EspFlasher(this, mBaudRate);
        connect(mEspFlasher, SIGNAL(progress(int)), this ,SLOT(onFlasherProgress(int)));
    }
}

void EspRom::clearFlasher() {
    if(mEspFlasher) {
        delete mEspFlasher;
        mEspFlasher = NULL;
    }
}

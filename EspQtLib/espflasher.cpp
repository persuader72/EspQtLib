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

#include "espflasher.h"
#include <QVector>
#include <QtEndian>
#include <QCryptographicHash>
#include <QThread>

//#define CESANTA_FLASHER_STUB ":/binary/cesanta.txt"
#define CESANTA_FLASHER_STUB ":/binary/stub_flasher.json"
// Default baudrate. The ROM auto-bauds, so we can use more or less whatever we waitnt.
#define ESP_ROM_BAUD    115200


#define CMD_FLASH_WRITE 1
#define CMD_FLASH_READ 2
#define CMD_FLASH_DIGEST 3
#define CMD_BOOT_FW 6


#define ERR_ReadError  "Read error"
#define ERR_ExpectedStatusCode  "Expected status, got %1"
#define ERR_ExpectedDigest "Expected digest, got: %1"
#define ERR_DigestMismatch "Digest mismatch got:%1 exprected:%2"
#define ERR_WrongArgument "Wrong argument: %1"
#define ERR_WriteFailure "Write failure, status: %1"
#define ERR_UnexpectedData "Unexpected data received"

EspFlasher::EspFlasher(EspRom *esp, quint32 baudRate) : QObject(esp), mEsp(esp), mRunStub(false) {
    qDebug("Running Cesanta flasher stub baud rate:%d", baudRate);
    if(baudRate <= ESP_ROM_BAUD) {
        baudRate = 0;
    }

    QVector<quint32> params(1,baudRate);
    Q_INIT_RESOURCE(resources);
    mEsp->runStub(CESANTA_FLASHER_STUB, params, false);

    if(baudRate > 0) {
        mEsp->setBaudRate(baudRate);
    }

    for(int i=0; i<20; i++) {
        mEsp->waitForReadyRead(10);
        if(mEsp->read()) {
            QByteArray p = mEsp->mLastPacket;
            if(p.contains("OHAI")) {
                mRunStub = true;
                qDebug("CesantaFlasher::CesantaFlasher stub loaded!");
            }
            continue;
        }
    }
}

QByteArray EspFlasher::flashRead(quint32 address, int size) {
    qDebug("CesantaFlasher::flashRead addr:%d size:%d", address, size);
    mEsp->write(QByteArray(1,CMD_FLASH_READ));
    mEsp->write(address, size, 32, 64);
    QByteArray memory;

    bool err = true;
    while(true) {
        mEsp->waitForReadyRead(10);
        if(mEsp->readTimeout(1000)) {
            memory.append(mEsp->mLastPacket);

            QByteArray dataSize(4, '\0');
            uchar *dataSizePtr = (uchar *)dataSize.data();
            qToLittleEndian(memory.size(), dataSizePtr);
            mEsp->write(dataSize);

            if(memory.size() == size) {
                err = false;
                break;
            } else if(memory.size() > size) {
                qDebug("CesantaFlasher::flashRead Read more than expected");
                break;
            }
        } else {
            qDebug("CesantaFlasher::flashRead read error");
            break;
        }
    }

    if(err) return QByteArray();

    err = true;
    if(mEsp->readTimeout(1000)) {
        QByteArray p = mEsp->mLastPacket;
        if(p.size() == 16) {
            QByteArray expectedDigest = QCryptographicHash::hash((memory),QCryptographicHash::Md5);
            if(expectedDigest != mEsp->mLastPacket) {
                setError(DigestMismatch, QString(ERR_DigestMismatch).arg(mEsp->lastPacketReaded().toHex().toUpper().constData()).arg(expectedDigest.toHex().toUpper().constData()));
            } else {
                err = false;
            }
        }
    }


    if(err) return QByteArray();

    err = true;
    if(mEsp->readTimeout(1000)) {
        QByteArray p = mEsp->mLastPacket;
        if(p.size() == 1) {
            quint8 statusCode = (quint8)p.at(0);
            if(statusCode == 0) {
                return memory;
            }
        }
    }

    return QByteArray();
}

bool EspFlasher::flashWrite(quint32 address, const QByteArray &data) {
    if(address % ESP_FLASH_SECTOR != 0) {
        setError(WrongArguments, QString(ERR_WrongArgument).arg("Address must be sector aligned. Current size is"));
        return false;
    }

    if(data.size() % ESP_FLASH_SECTOR) {
        setError(WrongArguments, QString(ERR_WrongArgument).arg("Size must be sector aligned" + QString::number(data.size())));
        return false;
    }

    mEsp->write(QByteArray(1,CMD_FLASH_WRITE));
    mEsp->write(address, data.size(), 1);

    int numSent = 0;
    int written = 0;

    while(written < data.size()) {
        if(mEsp->readTimeout(1000)) {
            if(mEsp->mLastPacket.size() == 4) {
                written = qFromLittleEndian(*(quint32 *)mEsp->mLastPacket.data());
                emit progress(written);
            } else if(mEsp->mLastPacket.size() == 1) {
                mStatusCode = (quint8)mEsp->mLastPacket.at(0);
                setError(WriteFailure, QString(ERR_WriteFailure).arg(mStatusCode));
                return false;
            } else {
                setError(UnexpectedData, QString(ERR_UnexpectedData));
                return false;
            }

            while(numSent < data.size() && numSent - written < 2048) {
                QByteArray portion = data.mid(numSent, 1024);
                qDebug("CesantaFlasher::flashWrite mEsp->write %d/%d,%d", numSent, written, portion.size());
                mEsp->portWrite(portion);
                numSent += 1024;
            }
        }
    }

    if(mEsp->readTimeout(1000)) {
        if(mEsp->mLastPacket.size() == 16) {
            QByteArray expectedDigest = QCryptographicHash::hash((data),QCryptographicHash::Md5);
            if(expectedDigest != mEsp->mLastPacket) {
                setError(DigestMismatch, QString(ERR_DigestMismatch).arg(mEsp->lastPacketReaded().toHex().toUpper().constData()).arg(expectedDigest.toHex().toUpper().constData()));
                return false;
            }
        } else {
            setError(ExpectedDigest, QString(ERR_ExpectedDigest).arg(mEsp->lastPacketReaded().toHex().toUpper().constData()));
            return false;
        }
    } else {
        setError(ReadError, QString(ERR_ReadError));
        return false;
    }

    if(mEsp->readTimeout(1000)) {
        if(mEsp->mLastPacket.size() == 1) {
            mStatusCode = (quint8)mEsp->mLastPacket.at(0);
        } else {
            setError(ExpectedStatusCode, QString(ERR_ExpectedStatusCode).arg(mEsp->lastPacketReaded().toHex().toUpper().constData()));
            return false;
        }
    } else {
        setError(ReadError, QString(ERR_ReadError));
        return false;
    }

    return mStatusCode == 0;
}

bool EspFlasher::flashDigest(QList<QByteArray> digests, quint32 address, quint32 size, quint32 digestBlockSize) {
    mEsp->write(CMD_FLASH_DIGEST);
    mEsp->write(address, size, digestBlockSize);

    while(true) {
        if(mEsp->readTimeout(1000)) {
            if(mEsp->lastPacketReaded().size() == 16) {
                digests.append(mEsp->lastPacketReaded());
            } else if(mEsp->lastPacketReaded().size() == 1) {
                setError(ExpectedStatusCode, QString(ERR_ExpectedStatusCode).arg(mEsp->lastPacketReaded().toHex().toUpper().constData()));
                mStatusCode = (quint8)mEsp->lastPacketReaded().at(0);
                break;
            } else {
                return false;
            }
        } else {
            setError(ReadError, QString(ERR_ReadError));
            return false;
        }
    }

    return mStatusCode == 0;
}

bool EspFlasher::bootFw() {
    mEsp->write(CMD_BOOT_FW);
    if(mEsp->readTimeout(1000)) {
        if(mEsp->lastPacketReaded().size() == 1) {
            mStatusCode = (quint8)mEsp->lastPacketReaded().at(0);
        } else {
            setError(ExpectedStatusCode, QString(ERR_ExpectedStatusCode).arg(mEsp->lastPacketReaded().toHex().toUpper().constData()) );
            return false;
        }
    } else {
        setError(ReadError, QString(ERR_ReadError));
        return false;
    }

    return mStatusCode == 0;
}

void EspFlasher::setError(EspFlasher::Errors error, const QString &message) {
    mLastErrorCode = error;
    mLastErrorMessage = message;
    qDebug("CesantaFlasher::setError[%d] %s", error, message.toLatin1().constData());
}

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

#include "espinterface.h"

#include "cesantaflasher.h"
#include "esprom.h"

EspInterface::EspInterface(const QString &port, quint32 baud, QObject *parent) : QThread(parent), mEsp(0) {
    mPort = port; mBaud = baud;
    connect(this,SIGNAL(started()),this,SLOT(onThreadStarted()));
    //connect(this,SIGNAL(finished()),this,SLOT(threadFinished()));
    start();
}

void EspInterface::chipId() {
    if(mEsp && mEsp->isPortOpen()) {
        mArgs.clear();
        startOperation(opChipId);
    }
}

void EspInterface::flashId() {
    if(mEsp && mEsp->isPortOpen()) {
        mArgs.clear();
        startOperation(opFlashId);
    }
}

void EspInterface::readFlash(quint32 address, quint32 size) {
    if(mEsp && mEsp->isPortOpen()) {
        mArgs.clear();
        mArgs.append(QVariant(address));
        mArgs.append(QVariant(size));
        startOperation(opReadFlash);
    }
}

void EspInterface::writeFlash(quint32 address, QByteArray &data) {
    if(mEsp && mEsp->isPortOpen()) {
        mArgs.clear();
        mArgs.append(QVariant(address));
        mArgs.append(QVariant(data));
        startOperation(opWriteFlash);
    }
}

void EspInterface::startOperation(EspInterface::EspOperations operation) {
    mOperation = operation;
    mMutex.lock();
    mOperationPending.wakeAll();
    mMutex.unlock();
}

void EspInterface::run() {
    qDebug("EspInterface::run thread start %s@%d",mPort.toLatin1().constData(),mBaud);
    mEsp = new EspRom(mPort, mBaud, 0);

    if(!mEsp->isPortOpen()) {
        emit operationCompleted(opPortOpen, 0);
        setLastError(mEsp->lastError());
    } else {
        while(mEsp->isPortOpen() && mOperation != opQuit) {
            mMutex.lock();
            mOperationPending.wait(&mMutex);
            mMutex.unlock();

            mOperationResult = false;
            mOperationData.clear();

            if(mOperation == opConnect) {
                mOperationResult = mEsp->connect();

            } else if(mOperation == opChipId) {
                quint32 chipid = mEsp->chipId();
                mOperationData.fromValue(chipid);
                mOperationResult = chipid != 0;

            } else if(mOperation == opFlashId) {
                quint32 flashid = mEsp->flashId();
                mOperationData = QVariant(flashid);
                mOperationResult = flashid != 0;

            } else if(mOperation == opReadFlash) {
                quint32 address = mArgs.at(0).toInt();
                quint32 size = mArgs.at(1).toInt();
                mOperationData = QVariant(mEsp->flashRead(address,size));
                mOperationResult = !mOperationData.isNull();

            } else if(mOperation == opWriteFlash) {
                quint32 address = mArgs.at(0).toInt();
                QByteArray data = mArgs.at(1).toByteArray();
                mOperationResult = mEsp->flashWrite(address, data);
            }

            if(!mOperationResult) setLastError(mEsp->lastError());
            emit operationCompleted(mOperation, mOperationResult);
        }

        delete mEsp;
    }

}

void EspInterface::onThreadStarted() {
    qDebug("EspInterface::onThreadStarted");
    // Dirty trick to wait that thread exec the first istructions
    thread()->msleep(100);
    // Connect with esp8266
    startOperation(opConnect);
}


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

#ifndef ESPINTERFACE_H
#define ESPINTERFACE_H

#include <QThread>
#include <QList>
#include <QPair>
#include <QWaitCondition>
#include <QMutex>
#include <QVariant>

typedef QList< QPair<quint32, QByteArray> > FlashBlob;

class EspRom;
class EspInterface : public QThread {
    Q_OBJECT
public:
    enum EspOperations {opPortOpen,opConnect,opChipId,opFlashId,opReadFlash,opWriteFlash,opQuit};
    EspInterface(const QString &port, quint32 baud, QObject *parent=0);
    QVariant operationResultData() const { return mOperationData; }
    void chipId();
    void flashId();
    void readFlash(quint32 address, quint32 size);
    void writeFlash(quint32 address, QByteArray &data);
    void startOperation(EspOperations operation);
    QString lastError() const { return mLastError; }
    void setLastError(const QString &error) { mLastError = error; }
protected:
    virtual void run();
private slots:
    void onThreadStarted();
private:
    QString mPort;
    int mBaud;
private:
    QMutex mMutex;
    QWaitCondition mOperationPending;
    EspOperations mOperation;
    QList<QVariant> mArgs;
    bool mOperationResult;
    QVariant mOperationData;
    EspRom *mEsp;
    QString mLastError;
signals:
    void operationCompleted(int operation, bool result);
};

#endif // ESPINTERFACE_H

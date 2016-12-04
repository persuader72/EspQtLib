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

#ifndef CESANTAFLASHER_H
#define CESANTAFLASHER_H
#include "esprom.h"

class EspFlasher : public QObject {
    Q_OBJECT
public:
    enum Errors { ReadError, UnexpectedData, ExpectedStatusCode, ExpectedDigest, DigestMismatch, WrongArguments, WriteFailure,  };
    EspFlasher(EspRom *esp, quint32 baudRate=0);
    QString lastError() const { return mLastErrorMessage; }
    QByteArray flashRead(quint32 address, int size);
    bool flashWrite(quint32 address, const QByteArray &data);
    bool flashDigest(QList<QByteArray> digests, quint32 address, quint32 size, quint32 digestBlockSize=0);
    bool bootFw();

private:
    void setError(Errors error, const QString &message);
private:
    EspRom *mEsp;
    bool mRunStub;
    //QByteArray mMemory;
    quint8 mStatusCode;
    Errors mLastErrorCode;
    QString mLastErrorMessage;
signals:
    void progress(int written);
};

#endif // CESANTAFLASHER_H

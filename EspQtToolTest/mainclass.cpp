/* ESP8266 ROM Bootloader C++/QT5 Utility
 *
 * Derived from esptool.py (https://github.com/themadinventor/esptool)
 * Copyright (C) 2014-2016 Fredrik Ahlberg, Angus Gratton, other contributors as noted.
 * Copyright (c) 2016 Stefano Pagnottelli
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 *this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "mainclass.h"

#include <esprom.h>
#include <espinterface.h>

#include <QDebug>
#include <QFile>
#include <QCoreApplication>
#include <QCommandLineParser>

MainClass::MainClass(const QString &portname, int baud, QObject *parent) : QObject(parent) {
    mEspInt = new EspInterface(portname, baud, this);
    connect(mEspInt, SIGNAL(operationCompleted(int,bool)),this,SLOT(onOperationTerminated(int,bool)));
}

void MainClass::chipId() {
    mEspInt->chipId();
}

void MainClass::chipIdDone() {
    QTextStream out(stdout);
    out << QString("Reading Id of Chip...\n");
    quint32 chipId = (quint32)mEspInt->operationResultData().toInt();
    out << QString("Chip ID: %1\n").arg(chipId, 8, 16, QChar('0'));
    qApp->exit();
}

void MainClass::readFlash(quint32 address, quint32 size) {
    mEspInt->readFlash(address, size);
}

void MainClass::readFlashDone(const QByteArray &data) {
    QString filename = "out.bin";
    QTextStream out(stdout);
    QFile file(filename);
    if(file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
        out << QString("Writed %1 bytes of flash memory on file %2\n").arg(data.size()).arg(filename);
    } else {
        out << QString("Failed to write file %1\n").arg(filename);
    }
    qApp->exit();
}

void MainClass::writeFlash(quint32 address, const QString &filename) {
    QTextStream out(stdout);
    QFile file(filename);

    if(file.open(QIODevice::ReadOnly)) {
        QByteArray data =  file.readAll();
        mEspInt->writeFlash(address, data);
    } else {
        out << QString("Failed to read file %1\n").arg(filename);
    }
}

void MainClass::writeFlashDone() {
    QTextStream out(stdout);
    out << QString("Writed X bytes to flash  memory\n");
    qApp->exit();
}

void MainClass::flashId() {
    mEspInt->flashId();
}

void MainClass::flashIdDone() {
    QTextStream out(stdout);
    out << QString("Reading Id of Flash...\n");
    quint32 flashid = (quint32)mEspInt->operationResultData().toInt();
    out << QString("Flash manufacturer: %1\n").arg(flashid & 0xFF, 2, 16, QChar('0'));
    out << QString("Flash device: %1%2\n").arg((flashid>>8) & 0xFF, 2, 16, QChar('0')).arg((flashid>>16) & 0xFF, 2, 16, QChar('0'));
    qApp->exit();
}

void MainClass::executeCommand() {
    QCommandLineParser parser;
    parser.setApplicationDescription("EspQtTool - Tool for read/write flash of esp8266");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(QStringList() << "p" << "port", QCoreApplication::translate("main", "Serial port name"), "name", "/dev/ttyUSB0"));
    parser.addOption(QCommandLineOption(QStringList() << "b" << "baud", QCoreApplication::translate("main", "Serial port baudrate"), "name", QString::number(115200)));
    parser.addPositionalArgument("command", QCoreApplication::translate("main", "Perform commands"));
    parser.process(*qApp);

    const QStringList args = parser.positionalArguments();

    if(args.size() < 1) {
        QTextStream out(stdout);
        out << QCoreApplication::translate("main", "You must provide one command.\n\n");
        out << parser.helpText();
        qApp->exit();
    } else {
        if(args.at(0) == "chip_id") {
            chipId();
        } else if(args.at(0) == "flash_id") {
            flashId();
        } else if(args.at(0) == "write_flash") {
            if(args.size() >= 3) {
                bool ok;
                quint32 address = args.at(1).mid(0,2)=="0x" ? args.at(1).toInt(&ok,16) : args.at(1).toInt(&ok,10);
                QString filename = args.at(2);
                writeFlash(address, filename);
            }
        } else if(args.at(0) == "read_flash") {
            if(args.size() >= 3) {
                bool ok;
                quint32 address = args.at(1).mid(0,2)=="0x" ? args.at(1).toInt(&ok,16) : args.at(1).toInt(&ok,10);
                quint32 size = args.at(2).mid(0,2)=="0x" ? args.at(2).toInt(&ok,16) : args.at(2).toInt(&ok,10);
                readFlash(address, size);
            }
        }

    }

}

void MainClass::onOperationTerminated(int op, bool res) {
    qDebug("MainClass::onOperationTerminated %d %d",op, res);

    if(res == false) {
        qApp->exit();
    } else {
        if(op == EspInterface::opConnect) {
            executeCommand();
        } else if(op == EspInterface::opChipId) {
            chipIdDone();
        } else if(op == EspInterface::opFlashId) {
            flashIdDone();
        } else if(op == EspInterface::opReadFlash) {
            readFlashDone(mEspInt->operationResultData().toByteArray());
        } else if(op == EspInterface::opWriteFlash) {
            writeFlashDone();
        }
    }
}

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>

#include "mainclass.h"

enum EspToolCommands {
    chipId,
    noCommand,
    flashId,
    readFlash,
    writeFlash
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("EspQtTool");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("EspQtTool - Tool for read/write flash of esp8266");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(QStringList() << "p" << "port", QCoreApplication::translate("main", "Serial port name"), "name", "/dev/ttyUSB0"));
    parser.addOption(QCommandLineOption(QStringList() << "b" << "baud", QCoreApplication::translate("main", "Serial port baudrate"), "name", QString::number(115200)));
    parser.addPositionalArgument("command", QCoreApplication::translate("main", "Perform commands"));
    parser.process(app);

    bool ok;
    QString portname = parser.value("port");
    int baudrate =  parser.value("baud").toInt(&ok);

    MainClass *mc = new MainClass(portname,  baudrate);
    Q_UNUSED(mc);
    return app.exec();

}

#ifndef MAINCLASS_H
#define MAINCLASS_H

#include <QObject>
#include <QTextStream>

//class EspRom;
class EspInterface;
class MainClass : QObject {
    Q_OBJECT
public:
    MainClass(const QString &portname, int baud, QObject *parent=0);
    void chipId();
    void chipIdDone();
    void readFlash(quint32 address, quint32 size);
    void readFlashDone(const QByteArray &data);
    void writeFlash(quint32 address, const QString &filename);
    void writeFlashDone();
    void flashId();
    void flashIdDone();
    void executeCommand();
private slots:
    void onOperationTerminated(int op, bool res);
private:
    EspInterface *mEspInt;
    //EspRom *mEsp;
    bool mConnected;
};

#endif // MAINCLASS_H

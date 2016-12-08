#ifndef FIRMWAREREPOSITORY_H
#define FIRMWAREREPOSITORY_H

#include <QObject>

class FatItem {
public:
    FatItem(quint32 address, const QString &name) : flashAddress(address), fileName(name) { }
public:
    quint32 flashAddress;
    QString fileName;
    QByteArray memoryData;
};

class FirmwareRepository : public QObject {
    Q_OBJECT
public:
    explicit FirmwareRepository(QObject *parent = 0);
    bool loadFromFile(const QString &filename, bool firmwareOnly);
    int repositoryFileIndex(const QString &name);
    const QList<FatItem> &items() const { return mList; }
    QString repositoryFileName() const;
    QString firmwareVersion() const { return mVersionString; }
private:
    void parseRepositoryFatFile(QByteArray &data, bool firmwareOnly);
    static bool fatItemLessThan(const FatItem &v1, const FatItem &v2) { return v1.flashAddress < v2.flashAddress; }
private:
    QString mFilePath;
    QList<FatItem> mList;
    QString mVersionString;
};

#endif // FIRMWAREREPOSITORY_H

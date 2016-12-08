#include "firmwarerepository.h"

#include <quazip.h>
#include <quazipfile.h>

#include <QTextStream>
#include <QFileInfo>

FirmwareRepository::FirmwareRepository(QObject *parent) : QObject(parent)
{

}

bool FirmwareRepository::loadFromFile(const QString &filename, bool firmwareOnly) {
    QuaZipFileInfo info;

    QuaZip quazip(filename);
    QuaZipFile file(&quazip);
    quazip.open(QuaZip::mdUnzip);

    mFilePath = filename;

    for(bool f=quazip.goToFirstFile(); f; f=quazip.goToNextFile()) {
        QString fileName = quazip.getCurrentFileName();
        if(fileName == "firmware_repository_fat.txt") {
            file.open(QIODevice::ReadOnly);
            quazip.getCurrentFileInfo(&info);
            QByteArray content = file.read(file.size());
            parseRepositoryFatFile(content, firmwareOnly);
            file.close();
        }

    }

    qSort(mList.begin(), mList.end(), fatItemLessThan);

    for(bool f=quazip.goToFirstFile(); f; f=quazip.goToNextFile()) {
        QString fileName = quazip.getCurrentFileName();
        int index = repositoryFileIndex(fileName);
        qDebug("FirmwareRepository::loadFromFile %d %s",index,fileName.toLatin1().constData());
        if(index != -1) {
            file.open(QIODevice::ReadOnly);
            quazip.getCurrentFileInfo(&info);
            QByteArray content = file.read(file.size());
            mList[index].memoryData = content;
            file.close();
        }
    }

    return true;
}

int FirmwareRepository::repositoryFileIndex(const QString &name) {
    int found = -1;
    for(int i=0;i<mList.size();i++) {
        if(mList.at(i).fileName == name) {
            found = i;
            break;
        }
    }
    return found;
}

QString FirmwareRepository::repositoryFileName() const {
    QFileInfo fi(mFilePath);
    return fi.baseName();
}

void FirmwareRepository::parseRepositoryFatFile(QByteArray &data, bool firmwareOnly) {
    bool ok;
    QTextStream ts(&data);

    while(!ts.atEnd()) {
        QString line = ts.readLine().trimmed();

        if(line.at(0)=='#') {
            mVersionString = line.mid(1).trimmed();
        } else if(line.contains(':')) {
            QStringList fields = line.split(':');
            if(fields.size()==2) {
                QString addressText = fields.at(0);
                quint32 address = addressText.toUInt(&ok, 16);
                QString fileName = fields.at(1);
                if(!firmwareOnly || fileName.left(4)=="user") mList.append(FatItem(address, fileName));
            }
        }
    }
}


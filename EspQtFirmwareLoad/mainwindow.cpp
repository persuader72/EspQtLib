#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <espinterface.h>

#include <quazip.h>
#include <quazipfile.h>

#include <QDebug>
#include <QFileDialog>
#include <QProgressBar>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), mEspInt(NULL), mCurrentRepoItem(0), mLastBytesWritten(0), mOldBytesWritten(0) {
    ui->setupUi(this);
    mProgress = new QProgressBar(this);
    mProgress->setMinimum(0);
    mProgress->setMaximum(1024);
    ui->statusBar->addWidget(mProgress);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::on_seleectRepo_clicked() {
    QuaZipFileInfo info;
    QString repoFIlePath = QFileDialog::getOpenFileName(this, tr("Select Repository"), QDir::currentPath(), QString("*.fwrepo"));

    if(!repoFIlePath.isNull()) {
        QuaZip quazip(repoFIlePath);
        QuaZipFile file(&quazip);
        quazip.open(QuaZip::mdUnzip);
        for(bool f=quazip.goToFirstFile(); f; f=quazip.goToNextFile()) {
            QString fileName = quazip.getCurrentFileName();
            if(fileName == "firmware_repository_fat.txt") {
                file.open(QIODevice::ReadOnly);
                quazip.getCurrentFileInfo(&info);
                QByteArray content = file.read(file.size());
                parseRepositoryFatFile(content);
                file.close();
            }

        }

        qSort(mRepository.begin(), mRepository.end(), fatItemLessThan);

        for(bool f=quazip.goToFirstFile(); f; f=quazip.goToNextFile()) {
            QString fileName = quazip.getCurrentFileName();
            int index = repositoryFileIndex(fileName);
            if(index != -1) {
                file.open(QIODevice::ReadOnly);
                quazip.getCurrentFileInfo(&info);
                QByteArray content = file.read(file.size());
                mRepository[index].memoryData = content;
                file.close();
            }
        }

        printReposistoryStats(repoFIlePath);
    }
}

int MainWindow::repositoryFileIndex(const QString &name) {
    int found = -1;
    for(int i=0;i<mRepository.size();i++) {
        if(mRepository.at(i).fileName == name) {
            found = i;
            break;
        }
    }
    return found;
}

void MainWindow::parseRepositoryFatFile(QByteArray &data) {
    bool ok;
    QTextStream ts(&data);

    while(!ts.atEnd()) {
        QString line = ts.readLine();
        if(line.contains(':')) {
            QStringList fields = line.split(':');
            if(fields.size()==2) {
                QString addressText = fields.at(0);
                quint32 address = addressText.toUInt(&ok, 16);
                QString fileName = fields.at(1);
                mRepository.append(FatItem(address, fileName));
            }
        }
    }
}

void MainWindow::printReposistoryStats(const QString &reponame) {
    mLastBytesWritten = mOldBytesWritten = 0;
    mProgress->setMaximum(repositoryBytesToWrite());
    ui->programStatusView->appendPlainText(QString("Memory segments in %1 size %2 bytes").arg(reponame).arg(mProgress->maximum()));
    for(int i=0;i<mRepository.size();i++) {
        const FatItem &item = mRepository.at(i);
        ui->programStatusView->appendPlainText(QString("0x%1 0x%2 %3").arg(item.flashAddress,6,16,QChar('0')).arg(item.memoryData.size(),5,16,QChar('0')).arg(item.fileName));
    }
}

int MainWindow::repositoryBytesToWrite() {
    int totSize = 0;
    for(int i=0;i<mRepository.size();i++) {
        const FatItem &item = mRepository.at(i);
        quint32 size = item.memoryData.size();
        if(size % 0x1000 != 0) size += 0x1000 - (size % 0x1000);
        totSize += size;
    }
    return totSize;
}

void MainWindow::on_flashDevice_clicked() {
    if(!mEspInt) {
        mEspInt = new EspInterface("/dev/ttyUSB0", 460800, this);
        connect(mEspInt, SIGNAL(operationCompleted(int,bool)),this,SLOT(onEspOperationTerminated(int,bool)));
        connect(mEspInt, SIGNAL(flasherProgress(int)), this, SLOT(onFlasherProgress(int)));
    } else {
        mEspInt->connectEsp();
    }
}

void MainWindow::onEspOperationTerminated(int op, bool res) {
    qDebug("MainWindow::onOperationTerminated %d %d",op, res);

    if(res == false) {
        ui->programStatusView->appendPlainText(QString("Error %1").arg(mEspInt->lastError()));
    } else {
        if(op == EspInterface::opConnect) {
            onEspConnected();
        } else if(op == EspInterface::opChipId) {
        } else if(op == EspInterface::opFlashId) {
        } else if(op == EspInterface::opReadFlash) {
        } else if(op == EspInterface::opWriteFlash) {
            onWriteFinished();
        }
    }
}

void MainWindow::onFlasherProgress(int written) {
    if(written < mLastBytesWritten) mOldBytesWritten += mLastBytesWritten;
    mProgress->setValue(mOldBytesWritten + written);
    mLastBytesWritten = written;
}

void MainWindow::onEspConnected() {
    mCurrentRepoItem = 0;
    while(mCurrentRepoItem<mRepository.size() && mRepository.at(mCurrentRepoItem).memoryData.isEmpty()) mCurrentRepoItem++;

    if(mCurrentRepoItem<mRepository.size()) {
        const FatItem &item = mRepository.at(mCurrentRepoItem);
        ui->programStatusView->appendPlainText(QString("%1 bytes at address %2 - %3").arg(item.memoryData.size(),5,16,QChar('0')).arg(item.flashAddress,6,16,QChar('0')).arg(item.fileName));
        mEspInt->writeFlash(item.flashAddress, item.memoryData, false);
    } else {
        onWriteFinished();
    }
}

void MainWindow::onWriteFinished() {
    mCurrentRepoItem += 1;
    while(mCurrentRepoItem<mRepository.size() && mRepository.at(mCurrentRepoItem).memoryData.isEmpty()) mCurrentRepoItem++;

    if(mCurrentRepoItem<mRepository.size()) {
        const FatItem &item = mRepository.at(mCurrentRepoItem);
        ui->programStatusView->appendPlainText(QString("%1 bytes at address %2 - %3").arg(item.memoryData.size(),5,16,QChar('0')).arg(item.flashAddress,6,16,QChar('0')).arg(item.fileName));
        QThread::msleep(50); // FIXME very bad line to syncronize thread
        mEspInt->writeFlash(item.flashAddress, item.memoryData, false);
    } else {
        onAllImageWrited();
    }

}

void MainWindow::onAllImageWrited() {
    //mCurrentRepoItem = 0;
    mEspInt->startOperation(EspInterface::opRebootFw);
}

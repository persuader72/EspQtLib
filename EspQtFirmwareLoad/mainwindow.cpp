#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <espinterface.h>

#include <quazip.h>
#include <quazipfile.h>

#include <QDebug>
#include <QFileDialog>
#include <QProgressBar>
#include <QSerialPortInfo>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), mEspInt(NULL), mCurrentRepoItem(0), mLastBytesWritten(0), mOldBytesWritten(0) {
    ui->setupUi(this);
    mProgress = new QProgressBar(this);
    mProgress->setMinimum(0);
    mProgress->setMaximum(1024);

    QList<QSerialPortInfo> serialports = QSerialPortInfo::availablePorts();
    for(int i=0;i<serialports.size();i++) {
        ui->serialPorts->addItem(serialports.at(i).portName());
    }

    int sel = 0;
    QList<qint32> baoudrates = QSerialPortInfo::standardBaudRates();
    for(int i=0;i<baoudrates.size();i++) {
        ui->baudRates->addItem(QString::number(baoudrates.at(i)));
        if(baoudrates.at(i)==115200) sel = i;
    }

    ui->baudRates->setCurrentIndex(sel);
    ui->statusBar->addWidget(mProgress);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::on_seleectRepo_clicked() {
    QuaZipFileInfo info;
    QString repoFIlePath = QFileDialog::getOpenFileName(this, tr("Select Repository"), QDir::currentPath(), QString("*.fwrepo"));

    if(!repoFIlePath.isNull()) {
        mRepository.loadFromFile(repoFIlePath, false);
        printReposistoryStats(repoFIlePath);
    }
}

void MainWindow::printReposistoryStats(const QString &reponame) {
    mLastBytesWritten = mOldBytesWritten = 0;
    mProgress->setValue(0);
    mProgress->setMaximum(repositoryBytesToWrite());

    ui->programStatusView->appendPlainText(QString("Memory segments in %1 size %2 bytes").arg(reponame).arg(mProgress->maximum()));
    for(int i=0;i<mRepository.items().size();i++) {
        const FatItem &item = mRepository.items().at(i);
        ui->programStatusView->appendPlainText(QString("0x%1 0x%2 %3").arg(item.flashAddress,6,16,QChar('0')).arg(item.memoryData.size(),5,16,QChar('0')).arg(item.fileName));
    }
}

int MainWindow::repositoryBytesToWrite() {
    int totSize = 0;
    for(int i=0;i<mRepository.items().size();i++) {
        const FatItem &item = mRepository.items().at(i);
        quint32 size = item.memoryData.size();
        if(size % 0x1000 != 0) size += 0x1000 - (size % 0x1000);
        totSize += size;
    }
    return totSize;
}

void MainWindow::on_flashDevice_clicked() {
    if(!mEspInt) {
        QString portName = ui->serialPorts->currentText();
        quint32 baudRate = ui->baudRates->currentText().toUInt();
        mEspInt = new EspInterface(portName, baudRate, this);
        connect(mEspInt, SIGNAL(operationCompleted(int,bool)),this,SLOT(onEspOperationTerminated(int,bool)));
        connect(mEspInt, SIGNAL(flasherProgress(int)), this, SLOT(onFlasherProgress(int)));
        mLastBytesWritten = mOldBytesWritten = 0;
        mProgress->setValue(0);
        setBusyState(true);
    } else {
        mEspInt->connectEsp();
    }
}

void MainWindow::onEspOperationTerminated(int op, bool res) {
    qDebug("MainWindow::onOperationTerminated %d %d",op, res);

    if(res == false) {
        ui->programStatusView->appendPlainText(QString("Error %1").arg(mEspInt->lastError()));
        setBusyState(false);
    } else {
        if(op == EspInterface::opConnect) {
            onEspConnected();
        } else if(op == EspInterface::opChipId) {
        } else if(op == EspInterface::opFlashId) {
        } else if(op == EspInterface::opReadFlash) {
        } else if(op == EspInterface::opWriteFlash) {
            onWriteFinished();
        } else if(op == EspInterface::opRebootFw) {
            onDeviceRebooted();
        } else if(op == EspInterface::opQuit) {
            mEspInt->deleteLater();
            mEspInt = 0;
            setBusyState(false);
        } else {

        }
    }
}

void MainWindow::onFlasherProgress(int written) {
    if(written < mLastBytesWritten) mOldBytesWritten += mLastBytesWritten;
    mProgress->setValue(mOldBytesWritten + written);
    mLastBytesWritten = written;
}

void MainWindow::setBusyState(bool busy) {
    if(busy) {
        ui->seleectRepo->setEnabled(false);
        ui->flashDevice->setEnabled(false);
    } else {
        ui->seleectRepo->setEnabled(true);
        ui->flashDevice->setEnabled(true);
    }
}

void MainWindow::onEspConnected() {
    mCurrentRepoItem = 0;
    while(mCurrentRepoItem<mRepository.items().size() && mRepository.items().at(mCurrentRepoItem).memoryData.isEmpty()) mCurrentRepoItem++;

    if(mCurrentRepoItem<mRepository.items().size()) {
        const FatItem &item = mRepository.items().at(mCurrentRepoItem);
        ui->programStatusView->appendPlainText(QString("%1 bytes at address %2 - %3").arg(item.memoryData.size(),5,16,QChar('0')).arg(item.flashAddress,6,16,QChar('0')).arg(item.fileName));
        mEspInt->writeFlash(item.flashAddress, item.memoryData, false);
    } else {
        onWriteFinished();
    }
}

void MainWindow::onWriteFinished() {
    mCurrentRepoItem += 1;
    while(mCurrentRepoItem<mRepository.items().size() && mRepository.items().at(mCurrentRepoItem).memoryData.isEmpty()) mCurrentRepoItem++;

    if(mCurrentRepoItem<mRepository.items().size()) {
        const FatItem &item = mRepository.items().at(mCurrentRepoItem);
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

void MainWindow::onDeviceRebooted() {
    mEspInt->quitThread();
}

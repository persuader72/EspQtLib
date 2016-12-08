#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QList>
#include <QPair>
#include <QMainWindow>

#include "firmwarerepository.h"

namespace Ui {
class MainWindow;
}

class QProgressBar;
class EspInterface;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
private slots:
    void on_seleectRepo_clicked();
    void on_flashDevice_clicked();
    void onEspOperationTerminated(int op, bool res);
    void onFlasherProgress(int written);
private:
    void setBusyState(bool busy);
    void onEspConnected();
    void onWriteFinished();
    void onAllImageWrited();
    void onDeviceRebooted();
private:
    void printReposistoryStats(const QString &reponame);
    int repositoryBytesToWrite();
private:
    Ui::MainWindow *ui;
    FirmwareRepository mRepository;
    EspInterface *mEspInt;
    int mCurrentRepoItem;
    int mLastBytesWritten;
    int mOldBytesWritten;
private:
    QProgressBar *mProgress;
};

#endif // MAINWINDOW_H

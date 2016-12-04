#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QList>
#include <QPair>
#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QProgressBar;
class EspInterface;
class FatItem {
public:
    FatItem(quint32 address, const QString &name) : flashAddress(address), fileName(name) { }
public:
    quint32 flashAddress;
    QString fileName;
    QByteArray memoryData;
};

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
    void onEspConnected();
    void onWriteFinished();
    void onAllImageWrited();
private:
    static bool fatItemLessThan(const FatItem &v1, const FatItem &v2) { return v1.flashAddress < v2.flashAddress; }
    int repositoryFileIndex(const QString &name);
    void parseRepositoryFatFile(QByteArray &data);
    void printReposistoryStats(const QString &reponame);
    int repositoryBytesToWrite();
private:
    Ui::MainWindow *ui;
    QList<FatItem> mRepository;
    EspInterface *mEspInt;
    int mCurrentRepoItem;
    int mLastBytesWritten;
    int mOldBytesWritten;
private:
    QProgressBar *mProgress;
};

#endif // MAINWINDOW_H

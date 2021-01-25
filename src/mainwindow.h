#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/***
 *
 * This file is part of Minecraft Bedrock Server Console software.
 *
 * It is licenced under the GNU GPL Version 3.
 *
 * A copy of this can be found in the LICENCE file
 *
 * (c) Ian Clark
 *
 **
*/
#include <QMainWindow>
#include <server/bedrockserver.h>
#include <backup/backupmanager.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event);

private:
    Ui::MainWindow *ui;
    BedrockServer *server;
    BackupManager *backups;
    bool shuttingDown;

    void setOptions();
    void setupUi();
    QString getServerRootFolder();
    bool serverLocationValid();
    void setBackupTimerActiveState(bool active);
    void setBackupTimerInterval(quint64 msec);
    void setBackupDelayLabel(int delay);
    void setBackupFrequencyLabel(int delayHours);

private slots:
    void handleServerOutput(BedrockServer::OutputType type, QString message);
    void handleServerStateChange(BedrockServer::ServerState newState);
    void setupBackupTimerLabel();
    void setupBackupStorageUsedLabel();
};
#endif // MAINWINDOW_H

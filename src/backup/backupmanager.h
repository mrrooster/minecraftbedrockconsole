#ifndef BACKUPMANAGER_H
#define BACKUPMANAGER_H
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
#include <server/bedrockserver.h>
#include <QObject>

class BackupManager : public QObject
{
    Q_OBJECT
public:
    explicit BackupManager(BedrockServer *server, QObject *parent = nullptr);

    enum FailReason { BackupFolderNotFound,Unknon };

    bool backupStorageFolderValid();
    void backupToFile(QString fileName);
    QDateTime getNextBackupTime();
    void setBackupFrequency(int value);
    qsizetype getBackupStorageFolderSize();
    qsizetype getMaximumStorageFolderSize();
    int getMaximumStorageFolderItemCount();
    int getMaximumStorageFolderItemAgeInDays();
    bool getStorageFolderSizeIsLimited();
    bool getStorageFolderAgeLimited();
    bool getStorageFolderItemCountLimited();

public slots:
    void setLimitStorageFolderSize(bool state);
    void setLimitStorageFolderItemAge(bool state);
    void setLimitStorageFolderItemCount(bool state);
    void setMaximumStorageFolderSize(qsizetype maxSizeInMib);
    void setMaximumStorageFolderItemCount(int count);
    void setMaximumStorageFolderItemAgeInDays(int days);
    void scheduleBackup();
    void setBackupStorageFolder(QString folder);
    void setEnableTimedBackups(bool state);
    void setBackupTimerIgnoresOtherEvents(bool state);

signals:
    void backupSavedToFile(QString filename);
    void backupFailed(FailReason reason);
    void backupTimerChanged();
    void backupStarting();
    void storageFolderItemsChanged();
    void backupFileDeleted(QString filename);

private:
    QString saveFileName; // If set then next backup saves here.
    QTimer backupTimer;
    BedrockServer *server;

    void setBackupTimerActiveState(bool active);
    void setBackupTimerInterval(quint64 msec);
    QString getBackupStorageFolder();
    void handlePruningBackups();
    void deleteFile(QString fileName);
};

#endif // BACKUPMANAGER_H

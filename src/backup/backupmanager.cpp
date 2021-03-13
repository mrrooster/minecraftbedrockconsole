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
#include "backupmanager.h"
#include <QSettings>
#include <QDir>
#include <QDebug>
#include <QDateTime>

#define BACKUP_PREFIX "server_backup_"

BackupManager::BackupManager(BedrockServer *server, QObject *parent) : QObject(parent), server(server)
{
    connect(this->server,&BedrockServer::backupFinished,this,[=](QString zipFile) {
        if (this->saveFileName!="") {
            QString destination = this->saveFileName;
            if (QFile::exists(destination)) {
                QFile::remove(destination);
            }
            QFile::copy(zipFile,destination);
            this->saveFileName="";
            emit backupSavedToFile(destination);
        } else {
            QSettings settings;
            QString backupFolder = getBackupStorageFolder();
            if (backupFolder!="" && QDir().exists(backupFolder)) {
                QString destination = backupFolder + "/"+ BACKUP_PREFIX + QDateTime::currentDateTimeUtc().toString("yyyyMMdd_hhmmss")+".zip";
                QFile::copy(zipFile,destination);
                emit backupSavedToFile(destination);
                emit storageFolderItemsChanged();
                handlePruningBackups();
            } else {
                emit backupFailed(BackupFolderNotFound);
            }
            setBackupTimerActiveState(settings.value("backup/doRegularBackups",false).toBool());
        }
        this->server->completeBackup(); // Always call this when you have a 'backupFinished' to delete temp files.
    });

    connect(this->server,&BedrockServer::backupStarting,this,[=](){
        if (this->saveFileName!="" && !QSettings().value("backup/alwaysBackupOnTime",false).toBool()) {
            /// Ad hoc backups don't reset the timer.
            setBackupTimerActiveState(false);
        }
        emit this->backupStarting();
    });
    connect(this->server,&BedrockServer::backupFailed,this,[=](){this->saveFileName="";});
    connect(this->server,&BedrockServer::backupComplete,this,[=](){this->saveFileName="";});

    connect(this->server,&BedrockServer::playerConnected,this,[=](QString, QString ) {
        if (QSettings().value("backup/backupOnJoin",false).toBool()) {
            this->server->scheduleBackup();
        }
    });
    connect(this->server,&BedrockServer::playerDisconnected,this,[=](QString, QString ) {
        if (QSettings().value("backup/backupOnLeave",false).toBool()) {
            this->server->scheduleBackup();
        }
    });

    // Timer
    this->backupTimer.setSingleShot(false);
    connect(&(this->backupTimer),&QTimer::timeout,this,[=]() {
        if (QSettings().value("backup/doRegularBackups",false).toBool()) {
            this->server->scheduleBackup();
        }
    });

    QSettings settings;
    int value = settings.value("backupFrequencyHours",3).toInt();
    setBackupTimerInterval(value * 1000 * 60 * 60);

    bool doRegularBackups = settings.value("doRegularBackups",false).toBool();
    setBackupTimerActiveState(doRegularBackups);

    handlePruningBackups();
}

bool BackupManager::backupStorageFolderValid()
{
    QString backupFolder = getBackupStorageFolder();
    return (backupFolder!="" && QDir().exists(backupFolder));
}

void BackupManager::backupToFile(QString fileName)
{
    this->saveFileName=fileName;
    this->server->startBackup();
}

void BackupManager::scheduleBackup()
{
    this->server->scheduleBackup();
}

void BackupManager::setBackupStorageFolder(QString folder)
{
    QSettings().setValue("backup/autoBackupFolder",folder);
}

QDateTime BackupManager::getNextBackupTime()
{
    QDateTime ret;
    if (this->backupTimer.isActive()) {
        ret = QDateTime::currentDateTime().addMSecs(this->backupTimer.remainingTime());
    }
    return ret;
}

void BackupManager::setBackupFrequency(int value)
{
    QSettings().setValue("backup/backupFrequencyHours",value);
    setBackupTimerInterval(value * 1000 * 60 * 60); // Interval is in hourLs
}

qsizetype BackupManager::getBackupStorageFolderSize()
{
    qsizetype size = 0;
    if (backupStorageFolderValid()) {
        QDir backupFolder(getBackupStorageFolder());

        QFileInfoList items = backupFolder.entryInfoList(QStringList() << BACKUP_PREFIX"*.zip");
        for(int x=0;x<items.size();x++) {
            size += items[x].size();
        }
    }
    return size;
}

qsizetype BackupManager::getMaximumStorageFolderSize()
{
    return (qsizetype)QSettings().value("backup/maximumStorageFolderSizeMiB",2048).toULongLong();
}

int BackupManager::getMaximumStorageFolderItemCount()
{
    return QSettings().value("backup/maximumStorageFolderItemCount",100).toInt();
}

int BackupManager::getMaximumStorageFolderItemAgeInDays()
{
    return QSettings().value("backup/maximumStorageFolderItemAgeDays",28).toInt();
}

bool BackupManager::getStorageFolderSizeIsLimited()
{
    return QSettings().value("backup/limitStorageFolderUsage",false).toBool();
}

bool BackupManager::getStorageFolderAgeLimited()
{
    return QSettings().value("backup/limitStorageFolderItemAge",false).toBool();
}

bool BackupManager::getStorageFolderItemCountLimited()
{
    return QSettings().value("backup/limitStorageFolderItemCount",false).toBool();
}

void BackupManager::setLimitStorageFolderSize(bool state)
{
    QSettings().setValue("backup/limitStorageFolderUsage",state);
}

void BackupManager::setLimitStorageFolderItemAge(bool state)
{
    QSettings().setValue("backup/limitStorageFolderItemAge",state);
}

void BackupManager::setLimitStorageFolderItemCount(bool state)
{
    QSettings().setValue("backup/limitStorageFolderItemCount",state);
}

void BackupManager::setMaximumStorageFolderSize(qsizetype maxSizeInMib)
{
    QSettings().setValue("backup/maximumStorageFolderSizeMiB",maxSizeInMib);
}

void BackupManager::setMaximumStorageFolderItemCount(int count)
{
    QSettings().setValue("backup/maximumStorageFolderItemCount",count);
}

void BackupManager::setMaximumStorageFolderItemAgeInDays(int days)
{
    QSettings().setValue("backup/maximumStorageFolderItemAgeDays",days);
}

void BackupManager::setEnableTimedBackups(bool state)
{
    QSettings().setValue("backup/doRegularBackups",state);
    this->setBackupTimerActiveState(state);
}

void BackupManager::setBackupTimerIgnoresOtherEvents(bool state)
{
    QSettings().setValue("backup/alwaysBackupOnTime",state);
}

void BackupManager::setBackupTimerActiveState(bool active)
{
    if (active) {
        if (!this->backupTimer.isActive()) {
            this->backupTimer.start();
        }
    } else {
        this->backupTimer.stop();
    }
    emit backupTimerChanged();
}

void BackupManager::setBackupTimerInterval(quint64 msec)
{
    if (msec < 60000) {
        msec = 60000; // Don't backup more frequently than a minute.
    }
    this->backupTimer.setInterval(msec);
    emit backupTimerChanged();
}

QString BackupManager::getBackupStorageFolder()
{
    return QSettings().value("backup/autoBackupFolder","").toString();
}

void BackupManager::handlePruningBackups()
{
    if (backupStorageFolderValid()) {
        QDir backupFolder(getBackupStorageFolder());

        if (getStorageFolderAgeLimited()) {
            int maxAge = getMaximumStorageFolderItemAgeInDays();
            qDebug() << "Pruning backups based on age. Age "<<maxAge<<"days";
            QFileInfoList items = backupFolder.entryInfoList(QStringList() << BACKUP_PREFIX"*.zip",QDir::NoFilter,QDir::Name|QDir::Reversed); // Reversed name sorting gives newest backup first...
            QDateTime now = QDateTime::currentDateTimeUtc();
            for(int x=0;x<items.size();x++) {
                //
                QString name = items[x].fileName().mid(QString(BACKUP_PREFIX).length());
                name = name.left(name.lastIndexOf('.'));
                if ( QDateTime::fromString(name,"yyyyMMdd_hhmmss").addDays(maxAge) < now ) {
                    // File is too old
                    deleteFile(items[x].absoluteFilePath());
                }
            }
        }

        if (getStorageFolderItemCountLimited()) {
            qDebug() << "Pruning backups based on count. Max "<<getMaximumStorageFolderItemCount();
            QFileInfoList items = backupFolder.entryInfoList(QStringList() << BACKUP_PREFIX"*.zip",QDir::NoFilter,QDir::Name|QDir::Reversed); // Reversed name sorting gives newest backup first...
            for(int x=getMaximumStorageFolderItemCount();x<items.size();x++) {
                deleteFile(items[x].absoluteFilePath());
            }
        }


        if (getStorageFolderSizeIsLimited()) {
            qDebug() << "Pruning backups based on size. Max backup folder size in MB: "<<getMaximumStorageFolderSize();
            // Prune based on size
            QFileInfoList items = backupFolder.entryInfoList(QStringList() << BACKUP_PREFIX"*.zip",QDir::NoFilter,QDir::Name|QDir::Reversed); // Reversed name sorting gives newest backup first...
            qsizetype maxSize = (getMaximumStorageFolderSize()*1024*1024);// maxSize now in bytes
            qsizetype size = 0;

            for(int x=0;x<items.size();x++) {
                size += items[x].size();
                if (size>maxSize) {
                    deleteFile(items[x].absoluteFilePath());
                }
            }
        }

    }
}

void BackupManager::deleteFile(QString fileName)
{
    qDebug() << "Deleting: "<< fileName;
    if (QFile(fileName).remove()) {
        emit backupFileDeleted(fileName);
    }
    emit storageFolderItemsChanged();
}

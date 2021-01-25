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
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QSettings>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow), shuttingDown(false)
{
    ui->setupUi(this);
    setWindowTitle(tr("Bedrock server console"));

    this->server = new BedrockServer(this);
    this->backups = new BackupManager(this->server, this);

    connect(this->server,&BedrockServer::serverOutput,this,&MainWindow::handleServerOutput);
    connect(this->server,&BedrockServer::serverStateChanged,this,&MainWindow::handleServerStateChange);

    connect(this->backups,&BackupManager::backupFailed,this,[=](BackupManager::FailReason reason) {
        if (reason==BackupManager::BackupFolderNotFound) {
            handleServerOutput(BedrockServer::ErrorOutput,QString(tr("The backup folder can not be found. Automatic backup failed.")));
        }
        this->ui->instantBackup->setEnabled(true);
    });
    connect(this->backups,&BackupManager::backupTimerChanged,this,&MainWindow::setupBackupTimerLabel);
    connect(this->backups,&BackupManager::backupSavedToFile,this,[=](QString file){
        handleServerOutput(BedrockServer::InfoOutput,QString(tr("Backup saved to: %1")).arg(file));
        this->ui->instantBackup->setEnabled(true);
    });
    connect(this->backups,&BackupManager::backupFileDeleted,this,[=](QString file){
        handleServerOutput(BedrockServer::InfoOutput,QString(tr("Backup deleted: %1")).arg(file));
    });
    connect(this->backups,&BackupManager::backupStarting,this,[=](){
        this->ui->instantBackup->setDisabled(true);
    });
    connect(this->backups,&BackupManager::storageFolderItemsChanged,this,[=]() {
        if (this->ui->tabWidget->currentIndex()==1) {
            this->setupBackupStorageUsedLabel();
        }
    });

    setupUi();

    this->handleServerStateChange(this->server->GetCurrentState());
    if (!this->backups->backupStorageFolderValid()) {
        handleServerOutput(BedrockServer::ErrorOutput,"The backup folder is not set correctly. Automatic backups will not work.");
    }

    // FOR NOW
    if (serverLocationValid()) {
        this->server->startServer();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if ( this->server->GetCurrentState() !=BedrockServer::ServerNotRunning && this->server->GetCurrentState() != BedrockServer::ServerStopped) {
        this->server->stopServer();
    } else {
        this->shuttingDown = true;
    }

    if (!this->shuttingDown) {
        shuttingDown=true;
        event->ignore();
    } else {
        QMainWindow::closeEvent(event);
    }
}

void MainWindow::handleServerOutput(BedrockServer::OutputType type, QString message)
{
    switch (type) {
    case BedrockServer::OutputType::ServerInfoOutput :
       this->ui->serverOutput->appendHtml(QString("<code>%1</code>").arg(message));
       break;
    case BedrockServer::OutputType::InfoOutput :
       this->ui->serverOutput->appendHtml(QString("<span style='color: #006de9;'><code>%1</code></span>").arg(message));
       break;
    case BedrockServer::OutputType::ErrorOutput :
       this->ui->serverOutput->appendHtml(QString("<span style='color: #e90b00;'><code>%1</code></span>").arg(message));
       break;
    case BedrockServer::OutputType::ServerStatus :
       this->ui->serverOutput->appendHtml(QString("<span style='color: #31ac0f;'><code>%1</code></span>").arg(message));
       break;
    default:
        this->ui->serverOutput->appendPlainText(message);
    }
    this->ui->serverOutput->ensureCursorVisible();
}

void MainWindow::handleServerStateChange(BedrockServer::ServerState newState)
{
    this->ui->serverState->setText(this->server->GetCurrentStateName());

    if (newState==BedrockServer::ServerStopped || newState==BedrockServer::ServerNotRunning) {
        this->ui->serverState->setStyleSheet("background-color: #ff9797;"); // Reddish
        if (shuttingDown) {
            this->close();
        }
    } else if (newState==BedrockServer::ServerLoading || newState==BedrockServer::ServerRestarting) {
        this->ui->serverState->setStyleSheet("background-color: #ffddb5;"); // Orangy
    } else if (newState==BedrockServer::ServerStartup || newState==BedrockServer::ServerShutdown) {
        this->ui->serverState->setStyleSheet("background-color: #fffdb5;"); // Yellowy
    } else if (newState==BedrockServer::ServerRunning) {
        this->ui->serverState->setStyleSheet("background-color: #ddffdd;"); // Green
    }

    this->ui->startServer->setEnabled( (newState==BedrockServer::ServerNotRunning || newState==BedrockServer::ServerStopped)  );
    this->ui->stopServer->setDisabled( (newState==BedrockServer::ServerNotRunning || newState==BedrockServer::ServerStopped)  );
}

void MainWindow::setOptions()
{
    QSettings settings;

    this->ui->serverFolder->setText(getServerRootFolder());
    this->ui->invalidServerLocationLabel->setHidden(serverLocationValid());

    settings.beginGroup("backup");
    this->ui->backupFolder->setText(settings.value("autoBackupFolder","").toString());
    this->ui->backupOnJoin->setChecked(settings.value("backupOnJoin").toBool());
    this->ui->backupOnLeave->setChecked(settings.value("backupOnLeave").toBool());
    this->ui->invalidBackupLocationLabel->setHidden(this->backups->backupStorageFolderValid());

    int value = settings.value("backupDelayMinutes",30).toInt();
    this->ui->backupDelaySlider->setValue(value);
    setBackupDelayLabel(value);
    this->server->setBackupDelaySeconds(value * 60);

    value = settings.value("backupFrequencyHours",3).toInt();
    this->ui->backupFrequencySlider->setValue(value);
    setBackupFrequencyLabel(value);

    bool doRegularBackups = settings.value("doRegularBackups",false).toBool();
    this->ui->alwaysBackupOnTime->setChecked(settings.value("alwaysBackupOnTime",false).toBool());
    this->ui->doRegularBackups->setChecked(doRegularBackups);
    this->ui->backupFrequencyLabel->setEnabled(doRegularBackups);
    this->ui->backupFrequencySlider->setEnabled(doRegularBackups);
    this->ui->alwaysBackupOnTime->setEnabled(doRegularBackups);
    this->ui->restrictBackupsStorageUsage->setChecked(this->backups->getStorageFolderSizeIsLimited());
    this->ui->storageUsage->setMinimumHeight(this->ui->storageUsedBar->height());
    this->ui->storageUsedBar->setVisible(this->backups->getStorageFolderSizeIsLimited());
    this->ui->maxStorageUsageMiB->setEnabled(this->backups->getStorageFolderSizeIsLimited());
    this->ui->maxStorageUsageMiB->setText(QString::number(this->backups->getMaximumStorageFolderSize()));
    this->ui->storageUsage->setHidden(this->backups->getStorageFolderSizeIsLimited());

    this->ui->restrictBackupAge->setChecked(this->backups->getStorageFolderAgeLimited());
    this->ui->restrictBackupAgeSlider->setEnabled(this->backups->getStorageFolderAgeLimited());
    this->ui->restrictBackupAgeSlider->setValue(this->backups->getMaximumStorageFolderItemAgeInDays()); // Causes slider to redraw if saved val == val in .ui file

    this->ui->restrictNumberOfBackups->setChecked(this->backups->getStorageFolderItemCountLimited());
    this->ui->restrictNumberOfBackupsAmount->setEnabled(this->backups->getStorageFolderItemCountLimited());
    this->ui->restrictNumberOfBackupsAmount->setText(QString::number(this->backups->getMaximumStorageFolderItemCount()));

    settings.endGroup();
}

void MainWindow::setupUi()
{
    this->ui->serverOutput->setMaximumBlockCount(20000);
    this->ui->commandEdit->setPlaceholderText(tr("Server command..."));

    connect(ui->startServer,&QPushButton::clicked,this->server,&BedrockServer::startServer);
    connect(ui->stopServer,&QPushButton::clicked,this->server,&BedrockServer::stopServer);

    connect(ui->instantBackup,&QPushButton::clicked,this,[=](){
        QString destination = QFileDialog::getSaveFileName(this,"Save backup to...","backup.zip","*.zip");
        this->backups->backupToFile(destination);
    });
    connect(ui->scheduleBackup,&QPushButton::clicked,this->backups,&BackupManager::scheduleBackup);

    connect(ui->sendButton,&QPushButton::clicked,this,[=]() {
        this->server->sendCommandToServer(this->ui->commandEdit->text());
        this->ui->commandEdit->clear();
    });

    // Settings
    connect(this->ui->selectServerFolder,&QPushButton::clicked,this,[=]() {this->ui->serverFolder->setText(QFileDialog::getExistingDirectory(this,tr("Select Minecraft Bedrock server folder"),"",QFileDialog::ShowDirsOnly));});
    connect(this->ui->serverFolder,&QLineEdit::textChanged,this,[=](QString text) {
        if (QDir(text).exists()) {
            QSettings().setValue("server/serverRootFolder",text);
            this->ui->invalidServerLocationLabel->setHidden(serverLocationValid());
            if (serverLocationValid()) {
                this->server->setServerRootFolder(text);
            }
        } else {
            this->ui->invalidServerLocationLabel->setVisible(true);
        }
    });
    connect(this->ui->selectBackupFolder,&QPushButton::clicked,this,[=]() {this->ui->backupFolder->setText(QFileDialog::getExistingDirectory(this,tr("Select folder for automatic backups"),"",QFileDialog::ShowDirsOnly));});
    connect(this->ui->backupFolder,&QLineEdit::textChanged,this,[=](QString text) {
        this->backups->setBackupStorageFolder(text);
        this->ui->invalidBackupLocationLabel->setHidden(this->backups->backupStorageFolderValid());
    });
    connect(this->ui->backupOnJoin,&QCheckBox::stateChanged,this,[=](bool state) { QSettings().setValue("backup/backupOnJoin",state);});
    connect(this->ui->backupOnLeave,&QCheckBox::stateChanged,this,[=](bool state) { QSettings().setValue("backup/backupOnLeave",state);});
    connect(this->ui->backupDelaySlider,&QSlider::valueChanged,this,[=](int value) {
        setBackupDelayLabel(value);
        QSettings().setValue("backup/backupDelayMinutes",value);
        this->server->setBackupDelaySeconds(value * 60);
    });
    connect(this->ui->backupFrequencySlider,&QSlider::valueChanged,this,[=](int value) {
        setBackupFrequencyLabel(value);
        this->backups->setBackupFrequency(value);
    });
    connect(this->ui->doRegularBackups,&QCheckBox::stateChanged,this->backups,&BackupManager::setEnableTimedBackups);
    connect(this->ui->alwaysBackupOnTime,&QCheckBox::stateChanged,this->backups,&BackupManager::setBackupTimerIgnoresOtherEvents);
    connect(this->ui->tabWidget,&QTabWidget::currentChanged,this,[=](int idx) {
       if (idx==1) { // Options page
           this->setupBackupStorageUsedLabel();
       }
    });
    connect(this->ui->restrictBackupsStorageUsage,&QCheckBox::stateChanged,this,[=](bool state) {
        this->backups->setLimitStorageFolderSize(state);
        setupBackupStorageUsedLabel();
    });
    connect(this->ui->maxStorageUsageMiB,&QLineEdit::textChanged,this,[=](QString text) {
       qsizetype newSize = text.toLongLong();
       this->backups->setMaximumStorageFolderSize(newSize);
       this->setupBackupStorageUsedLabel();
    });

    connect(this->ui->restrictNumberOfBackups,&QCheckBox::stateChanged,this->backups,&BackupManager::setLimitStorageFolderItemCount);
    connect(this->ui->restrictNumberOfBackupsAmount,&QLineEdit::textChanged,this,[=](QString value) {
        this->backups->setMaximumStorageFolderItemCount(value.toInt());
    });

    connect(this->ui->restrictBackupAge,&QCheckBox::stateChanged,this->backups,&BackupManager::setLimitStorageFolderItemAge);
    connect(this->ui->restrictBackupAgeSlider,&QSlider::valueChanged,this,[=](int value) {
        this->backups->setMaximumStorageFolderItemAgeInDays(value);
        this->ui->restrictBackupAge->setText(QString(tr("Delete backups older than %Ln day(s)","backup_age",value)));
    });


    setOptions();
    this->ui->restrictBackupAge->setText(QString(tr("Delete backups older than %Ln day(s)","backup_age",this->ui->restrictBackupAgeSlider->value())));
//    connect(this->ui->tabWidget,&QTabWidget::currentChanged,this,[=](int idx) { if (idx==1) {setOptions();}});
    this->ui->tabWidget->setCurrentIndex( serverLocationValid() ? 0 : 1 );
}

QString MainWindow::getServerRootFolder()
{
    return QSettings().value("server/serverRootFolder","").toString();
}

bool MainWindow::serverLocationValid()
{
    QString serverRoot = getServerRootFolder();
    QDir serverRootDir(serverRoot);

    return (serverRoot!="" && serverRootDir.exists() && serverRootDir.exists("bedrock_server.exe"));
}

void MainWindow::setBackupDelayLabel(int delay)
{
    this->ui->backupDelayLabel->setText(QString(tr("Wait at least %Ln minute(s) between backups","backup delay",delay)));
}

void MainWindow::setBackupFrequencyLabel(int delayHours)
{
    this->ui->backupFrequencyLabel->setText(QString(tr("%Ln hour(s)","backup frequency",delayHours)));
}

void MainWindow::setupBackupTimerLabel()
{
    QDateTime nextBackup = this->backups->getNextBackupTime();
    if (nextBackup.isValid()) {
        this->ui->nextBackupLabel->setText(QString(tr("Next scheduled backup: %1"))
                                           .arg(nextBackup.toString())
                                           );
    } else {
        this->ui->nextBackupLabel->setText("");
    }
}

void MainWindow::setupBackupStorageUsedLabel()
{
    this->ui->storageUsage->setText(QString(tr("%1 used","backup_data_size")).arg(QLocale().formattedDataSize(this->backups->getBackupStorageFolderSize())));
    qsizetype val = this->backups->getBackupStorageFolderSize()/1024/1024; // mib
    qsizetype max = this->backups->getMaximumStorageFolderSize(); // returnbs mib.
    this->ui->storageUsedBar->setMaximum(max<val ? val : max);
    this->ui->storageUsedBar->setValue(val);
    if (!this->backups->getStorageFolderSizeIsLimited()) {
        // Hide the progress bar
        if (this->ui->storageUsedBar->isVisible()) {
            this->ui->storageUsage->setMinimumHeight(this->ui->storageUsedBar->height());
        }
        this->ui->storageUsedBar->setVisible(false);
    } else {
        this->ui->storageUsedBar->setVisible(true);
    }
}


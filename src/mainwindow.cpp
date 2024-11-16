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
#include <QCloseEvent>
#include <QTextEdit>
#include <QDesktopServices>
#include <QGridLayout>
#include <QLineEdit>
#include <QComboBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow), shuttingDown(false), playerInfoWidget(nullptr)
{
    ui->setupUi(this);
    setWindowTitle(tr("Bedrock server console"));
    this->statusBarWidget = new QLabel();
    this->statusBar()->addWidget(this->statusBarWidget);

    this->server = new BedrockServer(this);
    this->backups = new BackupManager(this->server, this);

    ui->copyright->setText(QString("<style>a {color: green;}</style>Version %1<br/>Built using <a href='mcbc:/qt'>Qt</a>, licenced under the <a href='mcbc:/gpl'>GNU GPL v3</a>. Latest version on <a href='https://github.com/mrrooster/minecraftbedrockconsole'>github</a>.").arg(qApp->applicationVersion()));
    ui->copyright->setStyleSheet("font-size: 8pt; color: grey;");

    connect(ui->copyright,&QLabel::linkActivated,this,[=](QString url) {
       if (url=="mcbc:/qt") {
           qApp->aboutQt();
       } else if (url=="mcbc:/gpl") {
           QDialog dialog(this);

           dialog.setLayout(new QVBoxLayout);
           QWidget *w;

           w = new QLabel(&dialog);
           ((QLabel*)w)->setText(QString(tr("<style>a {color: green;}</style>%1 is licenced under the <a href='https://www.gnu.org/licenses/gpl-3.0.html'>GNU GPL version 3</a>.<br/><br/>For more information see <a href='https://www.gnu.org/licenses/licenses.html'>the GNU website</a>.")).arg(qApp->applicationName()));
           ((QLabel*)w)->setOpenExternalLinks(true);
           dialog.layout()->addWidget(w);

           w = new QTextEdit(&dialog);
           QFile gpl(":/LICENCE");
           gpl.open(QIODevice::ReadOnly);
           ((QTextEdit*)w)->setText(QString(gpl.readAll()));

           dialog.layout()->addWidget(w);

           w = new QLabel(&dialog);
           ((QLabel*)w)->setText(QString(tr("(c) Copyright Ian Clark.<br/>There is NO WARRENTY, either express or implied.")));
           w->setStyleSheet("color: grey; font-size: 8pt;");

           dialog.layout()->addWidget(w);
           dialog.setMaximumWidth(500);
           dialog.setMinimumWidth(500);

           dialog.setWindowTitle(tr("Licence information","Licence dialog title"));
           dialog.exec();
       } else {
           QDesktopServices::openUrl(url);
       }
    });

    ui->serverConsole->setServer(this->server);

    //connect(this->server,&BedrockServer::serverOutput,this,&MainWindow::handleServerOutput);
    connect(this->server,&BedrockServer::serverStateChanged,this,&MainWindow::handleServerStateChange);
    connect(this->server,&BedrockServer::serverDifficulty,this,[=](BedrockServer::ServerDifficulty d) {
        this->ui->difficultySlider->setValue((int)d);
        this->ui->difficultyLabel->setText(
                    (d==0) ? tr("Peaceful") :
                    (d==1) ? tr("Easy") :
                    (d==2) ? tr("Normal") :
                    (d==3) ? tr("Hard")
                           : tr("UNKNOWN")
                    );
    });

    connect(this->server,&BedrockServer::serverConfigurationUpdated,this,[=]() {
        this->setupServerProperties();
        emit this->server->serverOutput(BedrockServer::WarningOutput,tr("Server configuration updated, server may need to be restarted."));
    });

    connect(this->backups,&BackupManager::backupFailed,this,[=](BackupManager::FailReason reason) {
        if (reason==BackupManager::BackupFolderNotFound) {
            emit this->server->serverOutput(BedrockServer::ErrorOutput,QString(tr("The backup folder can not be found. Automatic backup failed.")));
        }
        this->ui->instantBackup->setEnabled(true);
    });
    connect(this->backups,&BackupManager::backupTimerChanged,this,&MainWindow::setupBackupTimerLabel);
    connect(this->backups,&BackupManager::backupSavedToFile,this,[=](QString file){
        emit this->server->serverOutput(BedrockServer::InfoOutput,QString(tr("Backup saved to: %1")).arg(file));
        this->ui->instantBackup->setEnabled(true);
    });
    connect(this->backups,&BackupManager::backupFileDeleted,this,[=](QString file){
        emit this->server->serverOutput(BedrockServer::InfoOutput,QString(tr("Backup deleted: %1")).arg(file));
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
        emit this->server->serverOutput(BedrockServer::ErrorOutput,"The backup folder is not set correctly. Automatic backups will not work.");
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

void MainWindow::handleServerStateChange(BedrockServer::ServerState newState)
{
    this->ui->serverState->setText(this->server->GetCurrentStateName());

    bool running=false;
    bool startable = false;
    if (newState==BedrockServer::ServerStopped || newState==BedrockServer::ServerNotRunning) {
        this->ui->serverState->setStyleSheet("background-color: #ff9797;"); // Reddish
        if (shuttingDown) {
            this->close();
        }
        startable = true;
    } else if (newState==BedrockServer::ServerLoading || newState==BedrockServer::ServerRestarting) {
        this->ui->serverState->setStyleSheet("background-color: #ffddb5;"); // Orangy
    } else if (newState==BedrockServer::ServerStartup || newState==BedrockServer::ServerShutdown) {
        this->ui->serverState->setStyleSheet("background-color: #fffdb5;"); // Yellowy
    } else if (newState==BedrockServer::ServerRunning) {
        this->ui->serverState->setStyleSheet("background-color: #ddffdd;"); // Green
        running=true;
        setupServerProperties();
    }

    this->ui->startServer->setEnabled( startable );
    this->ui->stopServer->setEnabled( running );
    this->ui->difficultySlider->setEnabled( running );
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
    connect(ui->startServer,&QPushButton::clicked,this->server,&BedrockServer::startServer);
    connect(ui->stopServer,&QPushButton::clicked,this->server,&BedrockServer::stopServer);
    connect(ui->restartAfter,&QPushButton::clicked,this,[=]() {
        if (this->server->pendingShutdownSeconds()>=0) {
            this->server->abortPendingShutdown();
        } else {
            this->server->restartServerAfter(this->ui->restartAfterSeconds->text().toInt()*1000);
        }
     });

    connect(ui->instantBackup,&QPushButton::clicked,this,[=](){
        QString destination = QFileDialog::getSaveFileName(this,"Save backup to...","backup.zip","*.zip");
        if (destination!="") {
            this->backups->backupToFile(destination);
        }
    });
    connect(ui->scheduleBackup,&QPushButton::clicked,this->backups,&BackupManager::scheduleBackup);

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
    connect(this->ui->doRegularBackups,&QCheckBox::stateChanged,this->ui->backupFrequencySlider,&QSlider::setEnabled);
    connect(this->ui->doRegularBackups,&QCheckBox::stateChanged,this->ui->backupFrequencySlider,&QSlider::setEnabled);
    connect(this->ui->doRegularBackups,&QCheckBox::stateChanged,this->backups,&BackupManager::setEnableTimedBackups);
    connect(this->ui->alwaysBackupOnTime,&QCheckBox::stateChanged,this->backups,&BackupManager::setBackupTimerIgnoresOtherEvents);
    connect(this->ui->tabWidget,&QTabWidget::currentChanged,this,[=](int idx) {
       if (idx==2) { // Options page
           this->setupBackupStorageUsedLabel();
       }
    });
    connect(this->ui->restrictBackupsStorageUsage,&QCheckBox::stateChanged,this,[=](bool state) {
        this->backups->setLimitStorageFolderSize(state);
        this->ui->maxStorageUsageMiB->setEnabled(state);
        setupBackupStorageUsedLabel();
    });
    connect(this->ui->maxStorageUsageMiB,&QLineEdit::textChanged,this,[=](QString text) {
       qsizetype newSize = text.toLongLong();
       this->backups->setMaximumStorageFolderSize(newSize);
       this->setupBackupStorageUsedLabel();
    });

    connect(this->ui->restrictNumberOfBackups,&QCheckBox::stateChanged,this->backups,&BackupManager::setLimitStorageFolderItemCount);
    connect(this->ui->restrictNumberOfBackups,&QCheckBox::stateChanged,this->ui->restrictNumberOfBackupsAmount,&QLineEdit::setEnabled);
    connect(this->ui->restrictNumberOfBackupsAmount,&QLineEdit::textChanged,this,[=](QString value) {
        this->backups->setMaximumStorageFolderItemCount(value.toInt());
    });

    connect(this->ui->restrictBackupAge,&QCheckBox::stateChanged,this->backups,&BackupManager::setLimitStorageFolderItemAge);
    connect(this->ui->restrictBackupAge,&QCheckBox::stateChanged,this->ui->restrictBackupAgeSlider,&QSlider::setEnabled);
    connect(this->ui->restrictBackupAgeSlider,&QSlider::valueChanged,this,[=](int value) {
        this->backups->setMaximumStorageFolderItemAgeInDays(value);
        this->ui->restrictBackupAge->setText(QString(tr("Delete backups older than %Ln day(s)","backup_age",value)));
    });

    connect(this->ui->difficultySlider,&QSlider::valueChanged,this->server,&BedrockServer::setDifficulty);

    // Player widget
    connect(this->ui->playerList,&QTreeView::clicked,this,[=](QModelIndex index) {
        if (this->playerInfoWidget!=nullptr) {
            this->playerInfoWidget->deleteLater();
            this->playerInfoWidget=nullptr;
        }
        QString xuid = this->server->getXuidFromIndex(index);
        if (xuid!="") {
            this->playerInfoWidget = new PlayerInfoWidget(this->server,xuid,this);
            this->ui->playerListLayout->addWidget(this->playerInfoWidget);
        }
    });

    connect(this->server,&BedrockServer::serverStatusLine,this,[=](QString status) {
       this->statusBarWidget->setText(status);
    });

    setOptions();
    this->ui->restrictBackupAge->setText(QString(tr("Delete backups older than %Ln day(s)","backup_age",this->ui->restrictBackupAgeSlider->value())));

    // Set the model for the treeview
    this->ui->playerList->setModel(this->server->getServerModel());


    this->ui->onlineBar->setServer(this->server);

//    connect(this->ui->tabWidget,&QTabWidget::currentChanged,this,[=](int idx) { if (idx==1) {setOptions();}});
    this->ui->tabWidget->setCurrentIndex( serverLocationValid() ? 0 : 2 );

    // Server properties
    connect(this->ui->saveConfig,&QPushButton::clicked,this->server,&BedrockServer::saveConfiguration);
    connect(this->ui->saveConfigAndRestart,&QPushButton::clicked,this,[=](){
        this->server->saveConfiguration();
        this->server->restartServerAfter(this->ui->restartAfterSeconds->text().toInt() * 1000);
    });

    // Restart delay
    connect(this->server,&BedrockServer::shutdownServerIn,this,[=](int sec) {
        this->ui->restartAfterSeconds->setVisible(sec<0);
        this->ui->restartAfterLabel->setVisible(sec<0);
        if (sec>=0) {
            this->ui->restartAfter->setText(tr("Stop restart (%1:%2)").arg(sec/60).arg(QString::number(sec%60),2,'0'));
        } else {
            this->ui->restartAfter->setText(tr("Restart after"));
        }
    });
    connect(this->ui->restartAfterSeconds,&QLineEdit::textChanged,this,[=](QString value) {
        bool parseable = false;
        int val = value.toInt(&parseable);
        if (parseable) {
            QSettings().setValue("server/restartDelaySeconds",val);
        }
    });
    this->ui->restartAfterSeconds->setText(QSettings().value("server/restartDelaySeconds",120).toString());
}

void MainWindow::setupServerProperties()
{
    if (this->server) {
        auto props = this->server->serverConfiguration();
        QWidget *w = new QWidget();
        QGridLayout *l = new QGridLayout(w);

        this->ui->serverPropertiesScrollArea->setWidget(w);
        //l->setRowWrapPolicy(QFormLayout::WrapAllRows);
        //l->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        int row=0;
        for(auto x=props.constBegin();x!=props.constEnd();x++) {
            BedrockServer::ConfigEntry *conf = (*x);

            QLabel *label = new QLabel();
            label->setWordWrap(true);
            label->setText(QString("<b>%1</b><br/><i>%2</i>").arg(conf->name).arg(conf->help));
            label->setToolTip(conf->help);
            label->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Minimum);
            QWidget *value;
            if (conf->value.metaType().id()==QMetaType::Bool) {
                QComboBox *valueBox = new QComboBox();
                valueBox->addItem("true");
                valueBox->addItem("false");
                valueBox->setCurrentIndex(conf->value.toBool()?0:1);

                connect(valueBox,&QComboBox::currentTextChanged,this,[=](QString text) {
                   conf->newValue = text;
                   if (conf->newValue!=conf->value) {
                       valueBox->setStyleSheet("color:blue;");
                   } else {
                       valueBox->setStyleSheet("color:black;");
                   }
                });

                value = valueBox;
            } else if (conf->possibleValues.size()>0) {
                QComboBox *valueBox = new QComboBox();
                valueBox->addItems(conf->possibleValues);
                valueBox->setCurrentIndex(conf->possibleValues.indexOf(conf->value.toString()));

                connect(valueBox,&QComboBox::currentTextChanged,this,[=](QString text) {
                   conf->newValue = text;
                   if (conf->newValue!=conf->value) {
                       valueBox->setStyleSheet("color:blue;");
                   } else {
                       valueBox->setStyleSheet("color:black;");
                   }
                });

                value = valueBox;
            } else {
                QLineEdit *valueEdit = new QLineEdit();
                valueEdit->setText(conf->value.toString());
                value = valueEdit;

                connect(valueEdit,&QLineEdit::textChanged,this,[=](QString text) {
                   conf->newValue = text;
                   if (conf->newValue!=conf->value) {
                       valueEdit->setStyleSheet("color:blue;");
                   } else {
                       valueEdit->setStyleSheet("color:black;");
                   }
                });
            }
            value->setMaximumWidth(300);
            value->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Maximum);

            l->addWidget(label,row,0);
            l->addWidget(value,row,1);
            row++;
        }
    }
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
    this->ui->doRegularBackups->setText(QString(tr("Schedule a backup at least every %Ln hour(s)","backup frequency",delayHours)));
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
            this->ui->storageUsage->setMaximumHeight(this->ui->storageUsedBar->height());
        }
        this->ui->storageUsedBar->setVisible(false);
        this->ui->storageUsage->setVisible(true);
    } else {
        this->ui->storageUsedBar->setVisible(true);
        this->ui->storageUsage->setVisible(false);
    }
}


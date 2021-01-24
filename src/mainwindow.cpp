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
    , ui(new Ui::MainWindow), promptToSaveNextBackup(false), shuttingDown(false)
{
    ui->setupUi(this);
    setWindowTitle(tr("Bedrock server console"));

    this->server = new BedrockServer(this);

    connect(this->server,&BedrockServer::serverOutput,this,&MainWindow::handleServerOutput);
    connect(this->server,&BedrockServer::serverStateChanged,this,&MainWindow::handleServerStateChange);

    connect(this->server,&BedrockServer::backupFinished,this,[=](QString zipFile) {
        if (this->promptToSaveNextBackup) {
            this->promptToSaveNextBackup=false;
            QString destination = QFileDialog::getSaveFileName(this,"Save backup to...","backup.zip","*.zip");
            if (destination!="") {
                if (QFile::exists(destination)) {
                    QFile::remove(destination);
                }
                QFile::copy(zipFile,destination);
            }
        } else {
            QSettings settings;
            QString backupFolder = settings.value("backup/autoBackupFolder","").toString();
            if (backupFolder!="" && QDir().exists(backupFolder)) {
                QString destination = backupFolder + "/server_backup_" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd_hhmmss")+".zip";
                QFile::copy(zipFile,destination);
            } else {
                handleServerOutput(BedrockServer::ErrorOutput,"The backup folder can not be found. Automatic backup failed.");
            }
            setBackupTimerActiveState(settings.value("backup/doRegularBackups",false).toBool());
        }
        this->server->completeBackup(); // Always call this when you have a 'backupFinished' to delete temp files.
    });

    connect(this->server,&BedrockServer::backupStarting,this,[=](){
        this->ui->instantBackup->setDisabled(true);
        if (!this->promptToSaveNextBackup && !QSettings().value("backup/alwaysBackupOnTime",false).toBool()) {
            /// Ad hoc backups don't reset the timer.
            setBackupTimerActiveState(false);
        }
    });
    connect(this->server,&BedrockServer::backupFailed,this,[=](){this->ui->instantBackup->setDisabled(false);});
    connect(this->server,&BedrockServer::backupComplete,this,[=](){this->ui->instantBackup->setDisabled(false);});

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

    setupUi();

    this->handleServerStateChange(this->server->GetCurrentState());
    if (!autoBackupLocationValid()) {
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
    this->ui->invalidBackupLocationLabel->setHidden(autoBackupLocationValid());

    int value = settings.value("backupDelayMinutes",30).toInt();
    this->ui->backupDelaySlider->setValue(value);
    setBackupDelayLabel(value);
    this->server->setBackupDelaySeconds(value * 60);

    value = settings.value("backupFrequencyHours",3).toInt();
    setBackupTimerInterval(value * 1000 * 60 * 60);
    this->ui->backupFrequencySlider->setValue(value);
    setBackupFrequencyLabel(value);

    bool doRegularBackups = settings.value("doRegularBackups",false).toBool();
    this->ui->doRegularBackups->setChecked(doRegularBackups);
    this->ui->alwaysBackupOnTime->setChecked(settings.value("alwaysBackupOnTime",false).toBool());
    setBackupTimerActiveState(doRegularBackups);
    this->ui->backupFrequencyLabel->setEnabled(doRegularBackups);
    this->ui->backupFrequencySlider->setEnabled(doRegularBackups);
    this->ui->alwaysBackupOnTime->setEnabled(doRegularBackups);
    settings.endGroup();
}

void MainWindow::setupUi()
{
    this->ui->serverOutput->setMaximumBlockCount(20000);

    connect(ui->startServer,&QPushButton::clicked,this->server,&BedrockServer::startServer);
    connect(ui->stopServer,&QPushButton::clicked,this->server,&BedrockServer::stopServer);
    connect(ui->instantBackup,&QPushButton::clicked,this,[=](){
        this->promptToSaveNextBackup=true;
        this->server->startBackup();
    });
    connect(ui->scheduleBackup,&QPushButton::clicked,this->server,&BedrockServer::scheduleBackup);

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
        if (QDir(text).exists()) {QSettings().setValue("backup/autoBackupFolder",text);}
        this->ui->invalidBackupLocationLabel->setHidden(autoBackupLocationValid());
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
        QSettings().setValue("backup/backupFrequencyHours",value);
        setBackupTimerInterval(value * 1000 * 60 * 60); // Interval is in hourLs
    });
    connect(this->ui->doRegularBackups,&QCheckBox::stateChanged,this,[=](bool state) {
        QSettings().setValue("backup/doRegularBackups",state);
        this->setBackupTimerActiveState(state);
    });
    connect(this->ui->alwaysBackupOnTime,&QCheckBox::stateChanged,this,[=](bool state) { QSettings().setValue("backup/alwaysBackupOnTime",state);});

    setOptions();
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

bool MainWindow::autoBackupLocationValid()
{
    QString backupFolder = QSettings().value("backup/autoBackupFolder","").toString();
    return (backupFolder!="" && QDir().exists(backupFolder));
}

void MainWindow::setBackupTimerActiveState(bool active)
{
    if (active) {
        if (!this->backupTimer.isActive()) {
            this->backupTimer.start();
        }
    } else {
        this->backupTimer.stop();
    }
    setupBackupTimerLabel();
}

void MainWindow::setBackupTimerInterval(quint64 msec)
{
    if (msec < 60000) {
        msec = 60000; // Don't backup more frequently than a minute.
    }
    this->backupTimer.setInterval(msec);
    setupBackupTimerLabel();
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
    if (this->backupTimer.isActive()) {
        this->ui->nextBackupLabel->setText(QString(tr("Next scheduled backup: %1"))
                                           .arg(QDateTime::currentDateTime().addMSecs(this->backupTimer.remainingTime()).toString())
                                           );
    } else {
        this->ui->nextBackupLabel->setText("");
    }
}


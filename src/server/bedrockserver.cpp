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
#include "bedrockserver.h"
#include "bedrockservermodel.h"
#include <QFileIconProvider>
#include <QTimer>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTextStream>

#include <QDebug>

BedrockServer::BedrockServer(QObject *parent) : QObject(parent),restartAfterStopped(false),tempDir(nullptr),state(ServerNotRunning),backupDelaySeconds(10),restartOnServerExit(true)
{
    this->serverRootFolder = "";
    this->serverProcess = new QProcess();
    // Tell QProcess to sent everything to stdout
    this->serverProcess->setProcessChannelMode(QProcess::MergedChannels);
    this->backupDelayTimer.setSingleShot(true);

    connect(this->serverProcess,&QProcess::readyReadStandardOutput,this,&BedrockServer::handleServerOutput);
    connect(this->serverProcess,&QProcess::stateChanged,this,[=](QProcess::ProcessState state) {
        if (state==QProcess::NotRunning && this->state==ServerShutdown) {
            setState(ServerStopped);
        }
        if (this->state==ServerRunning && state==QProcess::NotRunning && this->restartOnServerExit) {
            this->setState(ServerRestarting);
            emit this->serverOutput(OutputType::ErrorOutput,tr("Server stopped, will restart in 2 seconds..."));
            QTimer::singleShot(2000,this,[=]() {
                if (this->state!=ServerStopped) {
                    this->startServer();
                }
            });
        }
    });

    connect(&(this->backupDelayTimer),&QTimer::timeout,this,[=]() {
       if (this->backupScheduled) {
           this->startBackup();
       }
    });

    connect(&(this->startTimer),&QTimer::timeout,this,[=]() {
        if (this->state==ServerRunning) {
            this->stopAndRestartServer();
        } else {
            this->actuallyStartServer();
        }
    });

    connect(&(this->shutdownPendingTimer),&QTimer::timeout,this,[=]() {
        int ms = this->startTimer.remainingTime() ;
        QString message = (ms > 180000) // Greater than 3 minutes
                        ? tr("The server will restart in %1 minute(s).").arg((ms + 30000) / 1000 / 60)
                        : tr("The server will restart in %1 seconds(s).").arg((ms + 500 )/1000);
        this->sendCommandToServer(QString("say %1").arg(message));

        if (ms>600000) { // More than 10 minutes left
            this->shutdownPendingTimer.start(300000);
        } else if (ms>60000) { // More than a minute left
            this->shutdownPendingTimer.start(60000);
        } else if (ms>30000) {
            this->shutdownPendingTimer.start(30000);
        } else if (ms>1000) {
            this->shutdownPendingTimer.start(10000);
        } else {
            this->shutdownPendingTimer.stop();
        }
    });

    connect(&(this->shutdownHeartbeatTimer),&QTimer::timeout,this,[=]() {
        if (this->startTimer.isActive() == false) {
            emit shutdownServerIn(-1);
        } else {
            this->shutdownHeartbeatTimer.start(499);
            emit shutdownServerIn((this->startTimer.remainingTime() + 500)/1000);
        }
    });

    this->startTimer.setInterval(0);
    this->startTimer.setSingleShot(true);
    this->shutdownPendingTimer.setSingleShot(true);
    this->shutdownHeartbeatTimer.setSingleShot(true);

    connect(this,&QObject::destroyed,this->serverProcess,&QProcess::kill);

    this->model = new BedrockServerModel(this);
    connect(this->model,&BedrockServerModel::serverPermissionsChanged,this,&BedrockServer::serverPermissionsChanged);
}

QString BedrockServer::GetCurrentStateName()
{
    return stateName(this->state);
}

BedrockServer::ServerState BedrockServer::GetCurrentState()
{
    return this->state;
}

void BedrockServer::startBackup()
{
    this->backupScheduled=false;
    this->backupDelayTimer.stop();
    emit this->backupStarting();
    this->sendCommandToServer("save hold");
}

void BedrockServer::completeBackup()
{
    this->backupDelayTimer.start(this->backupDelaySeconds * 1000);
    emit this->backupComplete();
}

void BedrockServer::startServer()
{
    this->startTimer.stop();
    this->startTimer.setInterval(0);
    this->startTimer.start();
}

void BedrockServer::startServerAfter(int ms)
{
    emit this->serverOutput(InfoOutput,tr("Server will start in %1 seconds.").arg(ms/1000));
    this->startTimer.start(ms);
}

void BedrockServer::restartServerAfter(int ms)
{
    this->startServerAfter(ms);
    this->shutdownPendingTimer.start(0);
    this->shutdownHeartbeatTimer.start(0);
}

void BedrockServer::actuallyStartServer()
{
    this->restartAfterStopped = false;
    if (!serverRootIsValid()) {
        setState(ServerNotRunning);
        emit this->serverOutput(ErrorOutput,tr("Server root folder is not valid. Server can not start."));
    } else if (this->serverProcess->state()==QProcess::NotRunning) {
        loadConfiguration();
        this->maximumPlayerCount = getConfigValue("max-players").toInt();
        this->serverProcess->setProgram(this->serverRootFolder+"\\bedrock_server.exe");
        this->serverProcess->start();
        setState(ServerLoading);
    }
}

void BedrockServer::stopServer()
{
    if (state!=ServerNotRunning) {
        if (state==ServerShutdown) {
            // If you click too much on the stop button...
            if (this->serverProcess->state()==QProcess::Running || this->serverProcess->state()==QProcess::Starting) {
                this->serverProcess->kill();
            }
        }
        setState(ServerShutdown);
        sendCommandToServer("stop");
    }
}

void BedrockServer::stopAndRestartServer()
{
    this->restartAfterStopped=true;
    stopServer();
}

void BedrockServer::sendCommandToServer(QString command)
{
    this->serverProcess->write(QString(command+"\n").toLocal8Bit());
}

void BedrockServer::setDifficulty(int difficulty)
{
    if (difficulty>=0 && difficulty<=3 && this->difficulty!=(ServerDifficulty)difficulty) {
        sendCommandToServer(QString("difficulty %1").arg(QString::number(difficulty)));
        this->difficulty = (ServerDifficulty)difficulty;
        emit this->serverDifficulty(this->difficulty);
    }
}

QString BedrockServer::readServerLine()
{
    if(this->state==ServerLoading) {
        setState(ServerStartup);
    }
    QString line;
    int idx = this->processOutputBuffer.indexOf("\r\n");
    if (idx>-1) {
        line = this->processOutputBuffer.left(idx);
        this->processOutputBuffer.remove(0,idx+2);
        idx = this->processOutputBuffer.indexOf("\r\n");
    }
    return line;
}

QString BedrockServer::cleanServerLine(QString line)
{
    // Removes the timestamp log bit.
    int idx = -1;
    if (line.startsWith("[") && (idx = line.indexOf("]"))>-1) {
        // Assume there's a [2024-11-16 15:28:59:265 INFO] type prefix
        line = line.mid(idx+2);
    }
    return line;
}

bool BedrockServer::canReadServerLine()
{
    return (this->processOutputBuffer.indexOf("\r\n")>-1);
}

bool BedrockServer::serverHasData()
{
    return (this->processOutputBuffer.length()>0);
}

void BedrockServer::processRunningBackup()
{
    qDebug()<< "Backup in progress... sleeping for 1 second before polling server.";
    QTimer::singleShot(1000,this,[=](){
        sendCommandToServer("save query");
    });
}

void BedrockServer::processFinishedBackup()
{
   QStringList listOfFiles = readServerLine().split(", ");
   qDebug() << "Files to backup: "<<listOfFiles;

   if (this->tempDir) {
       delete this->tempDir;
   }
   this->tempDir = new QTemporaryDir();

   QString tempDirString = tempDir->path();
   qDebug() << "Temp dir: "<<tempDirString;
   QString worldName;
   if (tempDir->isValid()) {
       while(!listOfFiles.isEmpty()) {
           QList<QString> file = listOfFiles.takeFirst().split(":");

           qint64 destinationSize = file[1].toULongLong();
           QString destinationFileName = tempDir->path()+"/worlds/"+file[0];
           QString sourceFilename = this->serverRootFolder+"/worlds/"+file[0];
           QFile fileToCopy(sourceFilename);

           // Create any temp folders needed
           QString filePath = tempDirString+"/worlds/"+QFileInfo(file[0]).path();
           if (!QDir().mkpath(filePath)) {
               emit this->serverOutput(OutputType::ErrorOutput,QString(tr("Backup error when creating '%1'")).arg(filePath));
               emit this->backupFailed();
               sendCommandToServer("save resume");
               return;
           }
           if (!fileToCopy.exists()) {
               emit this->serverOutput(OutputType::ErrorOutput,QString(tr("Can't find file to copy: '%1'")).arg(sourceFilename));
               emit this->backupFailed();
               sendCommandToServer("save resume");
               return;
           }

           qDebug() << "Backing up"<<destinationSize<<"bytes of file: "<<fileToCopy.fileName()<<"to"<<destinationFileName;

           // Copy the data
           fileToCopy.copy(destinationFileName);
           // And make sure it's the correct size
           QFile::resize(destinationFileName,destinationSize);
       }

       // Now copy some config
       QStringList otherFiles = {"/server.properties","/whitelist.json","/permissions.json"};
       for(int x=0;x<otherFiles.size();x++) {
           qDebug() << "Backing up file: "<<this->serverRootFolder+otherFiles[x]<<"to"<<tempDir->path()+otherFiles[x];
           QFile::copy(this->serverRootFolder+otherFiles[x],tempDir->path()+otherFiles[x]);
       }

       emit this->serverOutput(OutputType::InfoOutput,tr("Finished copying data from the server."));
       emit this->serverOutput(OutputType::InfoOutput,tr("Requesting the server resume normal operations."));
       sendCommandToServer("save resume");
       emit backupSavingData();

       QProcess *zipper = new QProcess();

       QObject::connect(zipper,&QProcess::finished,this,&BedrockServer::handleZipComplete);
       QObject::connect(zipper,&QProcess::finished,zipper,&QObject::deleteLater);

       zipper->setProgram("powershell");
       QStringList zipperArguments;
       zipperArguments <<"Compress-Archive"<<"-Path"<<"\""+tempDir->path()+"/worlds/\"";

       for(int x=0;x<otherFiles.size();x++) {
            if (QFileInfo(tempDir->path()+otherFiles[x]).exists()) {
                zipperArguments << ",";
                zipperArguments << "\""+tempDir->path()+otherFiles[x]+"\"";
            }
       }

       zipperArguments<<"-DestinationPath"<<"\""+tempDir->path()+"/backup.zip\"";
       qDebug()<<"Zipper args: "<<zipperArguments;
       zipper->setArguments(zipperArguments);
       emit this->serverOutput(OutputType::InfoOutput,tr("Compressing the backup files."));
       zipper->start();

   } else {
       emit this->backupFailed();
       sendCommandToServer("save resume");
   }
}

void BedrockServer::parseOutputForEvents(QString output)
{
    if (output.contains("INFO] ")) {
        output = output.mid(output.indexOf("INFO] ")+6);
    }

    if (output.startsWith("Player connected:")) {
        // Player joined
        qDebug()<<"Player joined: "<<parsePlayerString(output.mid(18));
        QPair<QString,QString> player = parsePlayerString(output.mid(18));
        QString xuid = player.second;
        QString name = player.first;
        emit this->playerConnected(name,xuid);
        emitStatusLine();
    } else if (output.startsWith("Player disconnected: ")) {
        QPair<QString,QString> player = parsePlayerString(output.mid(21));
        QString xuid = player.second;
        QString name = player.first;
        emit this->playerDisconnected(name,xuid);
        emitStatusLine();
        qDebug()<<"Player left: "<<parsePlayerString(output.mid(21));
    } else if (output.startsWith("De-opped:")) {
        sendCommandToServer("permission list");
    } else if (output.startsWith("Opped:")) {
        sendCommandToServer("permission list");
    } else if (output.contains("Server started.")) {
        setState(ServerRunning);
    }
}

QPair<QString, QString> BedrockServer::parsePlayerString(QString playerString)
{
    QStringList bits = playerString.split(',');
    QString name = bits[0];
    QString xuid = bits[1].mid(7);
    return QPair<QString, QString>(name,xuid);
}

void BedrockServer::setState(BedrockServer::ServerState newState)
{
    if (this->state!=newState) {
        QString statusString = QString(tr("Server status changing: '%1' -> '%2'"))
                .arg(stateName(this->state))
                .arg(stateName(newState));
        emit this->serverOutput(ServerStatus,statusString);
        this->state = newState;
        if (newState != ServerRunning) {
            this->backupDelayTimer.stop();
        }
        emit this->serverStateChanged(newState);
        emitStatusLine();
        if (newState == ServerStopped && restartAfterStopped) {
            this->startServer();
        }
    }
}

bool BedrockServer::serverRootIsValid()
{
    QDir serverRoot(this->serverRootFolder);

    return (serverRoot.exists() && serverRoot.exists("bedrock_server.exe"));
}

void BedrockServer::processResponseBuffer()
{
    QJsonDocument doc = QJsonDocument::fromJson(this->responseBuffer.join("").toUtf8());

    if (doc.isObject()) {
        QString command = doc.object().value("command").toString();

        if (command=="permissions") {
            QJsonArray permissions = doc.object().value("result").toArray();
            QStringList ops,members,visitors;
            for(int x=0;x<permissions.size();x++) {
                QJsonObject permJson = permissions[x].toObject();
                QString xuid = permJson.value("xuid").toString();
                QString perm =  permJson.value("permission").toString();
                if (perm=="operator") {
                    ops.append(xuid);
                } else if (perm=="member") {
                    members.append(xuid);
                } else if (perm=="visitor") {
                    visitors.append(xuid);
                }
            }
            emit this->serverPermissionList(ops,members,visitors);
        } else if (command=="whitelist") {
            QStringList whitelistedUsers;

            QJsonArray results = doc.object().value("result").toArray();
            emit this->serverOutput(OutputType::InfoOutput,tr("Server whitelist:"));
            for(int x=0;x<results.size();x++) {
                QJsonObject result = results[x].toObject();
                QString name = result.value("name").toString();
                bool ignoresPlayerLimit = result.value("ignoresPlayerLimit").toBool();
                whitelistedUsers.append(name);
                emit this->serverOutput(OutputType::InfoOutput,tr("%1 %2").arg(QString(name)).arg(ignoresPlayerLimit ? " [Ignores player limit]" : ""));
            }
            emit this->serverWhitelist(whitelistedUsers);
        } else if (command=="ops") {
            QJsonArray results = doc.object().value("result").toArray();
            emit this->serverOutput(OutputType::InfoOutput,tr("Server operators:"));
            for(int x=0;x<results.size();x++) {
                QString xuid = results[x].toString();
                QString name = getPlayerNameFromXuid(xuid);
                if (name!=xuid) {
                    name = name + " ["+xuid+"]";
                }
                emit this->serverOutput(OutputType::InfoOutput,QString(name));
            }
        }
    }
    this->responseBuffer.clear();
}

void BedrockServer::emitStatusLine()
{
    QString state;
    bool online = false;
    switch (this->state) {
    case ServerNotRunning:
    case ServerStopped:
        state = tr("Server is not running");
        break;
    case ServerStartup:
    case ServerLoading:
    case ServerRestarting:
        state = tr("Server is starting up");
        break;
    case ServerShutdown:
        state = tr("Server is shutting down");
        break;
    case ServerRunning:
        state = tr("Server is running normally");
        online = true;
        break;
    default:
        state = tr("The state of the server is unknown");
    }

    QString onlineCount;
    if (online) {
        onlineCount=tr(", there are %Ln user(s) online.","status_text",this->model->onlinePlayerCount());
    }
    emit this->serverStatusLine(tr("%1%2","statusline").arg(state).arg(onlineCount));
}

void BedrockServer::loadConfiguration()
{
    QFile config(this->serverRootFolder+"/server.properties");

    if (!(config.exists() && config.open(QIODevice::ReadOnly|QIODevice::Text))) {
        return;
    }

    for(auto i=serverConfig.constBegin();i!=serverConfig.constEnd();i++) {
        delete (*i);
    }
    serverConfig.clear();
    serverConfigByName.clear();

    QTextStream lines(&config);
    ConfigEntry *entry=nullptr;

    while (!lines.atEnd()) {
        QString line = lines.readLine().trimmed();
        if (line=="") {
            continue;
        }
        if (line.startsWith('#') && entry) {
            entry->help += line.mid(1).trimmed()+'\n';
        } else if (line.contains('=')) {
            if (entry) {
                qDebug() << "Appending: " << entry->name << entry->value << entry->help;
                this->serverConfig.append(entry);
                this->serverConfigByName.insert(entry->name,entry);
            }
            entry = new ConfigEntry;

            entry->name = line.left(line.indexOf('='));
            QString value = line.mid(line.indexOf('=')+1);

            switch (getTypeOfConfigValue(entry->name)) {
                case String : entry->value = QVariant(value);break;
                case Integer : entry->value = QVariant(value.toInt());break;
                case Boolean : entry->value = QVariant(value == "true");break;
                case Float : entry->value = QVariant(value.toDouble());break;
            }

            entry->possibleValues = getPossibleValues(entry->name);
        }
    }
    if (entry) {
        qDebug() << "Appending: " << entry->name << entry->value << entry->help;
        this->serverConfig.append(entry);
        this->serverConfigByName.insert(entry->name,entry);
    }

    config.close();
}

void BedrockServer::saveConfiguration()
{
    qDebug()<<"Saving configuration.";

    emit this->serverOutput(OutputType::InfoOutput,tr("Saving server configuration."));
    QFile config(this->serverRootFolder+"/server.properties");

    if (!(config.exists() && config.open(QIODevice::ReadWrite|QIODevice::Text))) {
        return;
    }

    QTextStream lines(&config);
    QStringList outputLines;

    while (!lines.atEnd()) {
        QString line = lines.readLine();

        if (!line.startsWith('#') && line.contains('=')) {
            QString name = line.left(line.indexOf('='));
            if (serverConfigByName.contains(name)) {
                ConfigEntry *entry = serverConfigByName.value(name);
                if (!entry->newValue.isNull() && (entry->value.toString() != entry->newValue.toString())) {
                    line = QString("%1=%2").arg(name).arg(entry->newValue.toString());
                    entry->value = entry->newValue;
                }
            }
        }
        outputLines.append(line);
    }

    lines.seek(0);

    while(!outputLines.isEmpty()) {
        lines << outputLines.takeFirst() << "\n";
    }

    config.close();
    emit this->serverConfigurationUpdated();
}

BedrockServer::ConfigValueType BedrockServer::getTypeOfConfigValue(QString name)
{
    if (QStringList({"server-authoritative-block-breaking","correct-player-movement","content-log-file-enabled","texturepack-required","white-list","online-mode","allow-cheats","force-gamemode"}).contains(name)) {
        return BedrockServer::ConfigValueType::Boolean;
    } else if (QStringList({"max-player","server-port","server-portv6","view-distance","tick-distance","player-idle-timeout","max-threads","compression-threshold","player-movement-score-threshold"}).contains(name)) {
        return BedrockServer::ConfigValueType::Integer;
    } else if (name=="player-movement-distance-threshold") {
        return BedrockServer::ConfigValueType::Float;
    }
    return BedrockServer::ConfigValueType::String;
}

QStringList BedrockServer::getPossibleValues(QString name)
{
    if (name=="gamemode") {
        return {"survival", "creative",  "adventure"};
    } else if (name=="difficulty") {
        return {"peaceful","easy","normal","hard"};
    } else if (name=="default-player-permission-level") {
        return {"visitor", "member", "operator"};
    } else if (name=="server-authoritative-movement") {
        return {"client-auth", "server-auth", "server-auth-with-rewind"};
    }
    return QStringList();
}

QVariant BedrockServer::getConfigValue(QString name)
{
    for(auto i=this->serverConfig.constBegin();i!=this->serverConfig.constEnd();i++) {
        if ((*i)->name == name) {
            return (*i)->value;
        }
    }
    return QVariant();
}

QString BedrockServer::stateName(BedrockServer::ServerState state)
{
    switch (state) {
    case ServerLoading : return tr("Loading","server_state");
    case ServerNotRunning : return tr("Not running","server_state");
    case ServerRestarting : return tr("Restarting","server_state");
    case ServerRunning : return tr("Running","server_state");
    case ServerStartup : return tr("Starting","server_state");
    case ServerStopped : return tr("Stopped","server_state");
    case ServerShutdown: return tr("Shutdown","server_state");
    }
    return "UNKNOWN";
}

void BedrockServer::setServerRootFolder(QString folder)
{
    if (folder!=this->serverRootFolder && QDir(folder).exists()) {
        // New location is valid.
        stopServer(); // Incase we have a current server and it's running.
        this->serverRootFolder = folder;
        qDebug()<<"Server root folder set to: "<<folder;
    }
}

void BedrockServer::setBackupDelaySeconds(int seconds)
{
    this->backupDelaySeconds = seconds;
    if (this->backupDelayTimer.isActive() && (this->backupDelayTimer.remainingTime()/1000) > this->backupDelaySeconds) {
        this->backupDelayTimer.start(this->backupDelaySeconds*1000);
    }
}

QAbstractItemModel *BedrockServer::getServerModel()
{
    return this->model;
}

QString BedrockServer::getXuidFromIndex(QModelIndex index)
{
    return this->model->getXuidFromIndex(index);
}

QString BedrockServer::getPlayerNameFromXuid(QString xuid)
{
    return this->model->getPlayerNameFromXuid(xuid);
}

bool BedrockServer::isOnline(QString xuid)
{
    return this->model->isOnline(xuid);
}

int BedrockServer::getPermissionLevel(QString xuid)
{
    return this->model->getPermissionLevel(xuid);
}

void BedrockServer::setPermissionLevelForUser(QString xuid, BedrockServer::PermissionLevel level)
{
    if (level==getPermissionLevel(xuid)) {
        return;
    }

    QFile permissionsFile(QString("%1/permissions.json").arg(this->serverRootFolder));
    if (!permissionsFile.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonDocument permJson = QJsonDocument::fromJson(permissionsFile.readAll());
    permissionsFile.close();

    QString permString = level == 0 ? "member" : level == 1 ? "operator" : "Visitor";

    if (permJson.isArray()) {
        QJsonArray permissions = permJson.array();
        bool updated = false;
        for(int x=0;x<permissions.size();x++) {
            QJsonObject permObj = permissions.at(x).toObject();
            if (permObj.value("xuid").toString()==xuid) {
                // Found person in permissions array
                permObj["permission"]=permString;
                updated=true;
            }
            permissions[x]=permObj;
        }
        if (!updated) {
            QJsonObject newPerm;
            newPerm.insert("permission",permString);
            newPerm.insert("xuid",xuid);
            permissions.append(newPerm);
        }
        if (permissionsFile.open(QIODevice::WriteOnly)) {
            permissionsFile.resize(0);
            permJson.setArray(permissions);
            permissionsFile.write(permJson.toJson());
            permissionsFile.close();
            qDebug()<< "Saved updated permissions file, sending reload to server";
            sendCommandToServer("permission reload");
            sendCommandToServer("permission list");
        }
    }
}

QList<BedrockServer::ConfigEntry *> BedrockServer::serverConfiguration()
{
    return this->serverConfig;
}

int BedrockServer::maxPlayers()
{
    return this->maximumPlayerCount;
}

void BedrockServer::scheduleBackup()
{
    if (this->backupDelayTimer.isActive()) {
        // A backup has recently finished.
        this->backupScheduled = true;
        this->serverOutput(InfoOutput,QString(tr("Backup scheduled in cooldown, next backup is: %1, (%2 seconds away)."))
                           .arg(QDateTime::currentDateTime().addMSecs(this->backupDelayTimer.remainingTime()).toString())
                           .arg(this->backupDelayTimer.remainingTime()/1000)
                           );
    } else {
        this->startBackup();
    }
}

void BedrockServer::handleServerOutput()
{
    QByteArray serverOutputData = this->serverProcess->readAll();
    this->processOutputBuffer.append(serverOutputData);

    while(canReadServerLine()) {
        QString line = readServerLine();
        QString cleanLine = cleanServerLine(line);

        if (cleanLine=="Saving...") {
            emit backupStarting();
            emit this->serverOutput(OutputType::InfoOutput,tr("Server is preparing for the world files to be copied."));
            this->processRunningBackup();
        } else if (cleanLine=="A previous save has not been completed.") {
            emit this->serverOutput(OutputType::InfoOutput,tr("Server is still preparing for the world files to be copied."));
            emit backupInProgres();
            this->processRunningBackup();
        } else if (cleanLine=="Data saved. Files are now ready to be copied.") {
            emit this->serverOutput(OutputType::InfoOutput,tr("Server is ready for the world files to be copied."));
            emit backupSavingData();
            this->processFinishedBackup();
        } else if (cleanLine=="Changes to the level are resumed.") {
            emit this->serverOutput(OutputType::InfoOutput,tr("The server has resumed normal operations."));
            emit backupFinishedOnServer();
        } else if (line.contains("Difficulty: ") && this->state==ServerStartup) {
            QString difficulty = line.mid(line.indexOf("Difficulty: ")+12,1);
            this->difficulty=(ServerDifficulty)difficulty.toInt();
            emit this->serverDifficulty(this->difficulty);
        } else if (line.startsWith("###* ")) {
            this->responseBuffer.clear();
            this->responseBuffer.append(line.mid(5));
        } else if (line==" *###") {
            processResponseBuffer();
        } else {
            emit this->serverOutput(OutputType::ServerInfoOutput,line);
            parseOutputForEvents(line);
        }
    }
}

void BedrockServer::handleZipComplete()
{
    qDebug() << "File zipped up.";
    QFileInfo  zipFile(this->tempDir->path()+"/backup.zip");

    if (zipFile.exists()) {
        emit this->serverOutput(OutputType::InfoOutput,tr("Backup complete."));
        emit this->backupFinished(zipFile.canonicalFilePath());
    } else {
        emit this->serverOutput(OutputType::ErrorOutput,tr("Backup failed, zip not created."));
        emit this->backupFailed();
        delete this->tempDir;
        this->tempDir=nullptr;
    }
}

int BedrockServer::pendingShutdownSeconds()
{
    if (this->startTimer.remainingTime()>0 && state==ServerRunning) {
        return this->startTimer.remainingTime()/1000;
    }
    return -1;
}

void BedrockServer::abortPendingShutdown()
{
    this->startTimer.stop();
    this->shutdownPendingTimer.stop();
    this->shutdownHeartbeatTimer.stop();
    this->restartAfterStopped = false;
    emit shutdownServerIn(-1);
}


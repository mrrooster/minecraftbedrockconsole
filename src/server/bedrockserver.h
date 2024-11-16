#ifndef BEDROCKSERVER_H
#define BEDROCKSERVER_H

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
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QAbstractItemModel>
#include <QStandardItemModel>

class BedrockServerModel;

class BedrockServer : public QObject
{
    Q_OBJECT
public:
    explicit BedrockServer(QObject *parent = nullptr);

    enum OutputType { ServerInfoOutput,ServerErrorOutput,ServerStatus,InfoOutput,ErrorOutput,WarningOutput };
    enum ServerState { ServerLoading,ServerStartup,ServerRunning,ServerNotRunning,ServerStopped,ServerShutdown,ServerRestarting };
    enum ServerDifficulty { Peacefull,Easy,Normal,Hard };
    enum PermissionLevel { Member,Operator,Visitor };

    class ConfigEntry {
    public:
        QString name;
        QVariant value;
        QVariant newValue;
        QStringList possibleValues;
        QString help;
    };

    QString GetCurrentStateName();
    ServerState GetCurrentState();
    QString stateName(ServerState state);
    void setServerRootFolder(QString folder);
    void setBackupDelaySeconds(int seconds);
    QAbstractItemModel *getServerModel();
    QString getXuidFromIndex(QModelIndex index);
    QString getPlayerNameFromXuid(QString xuid);
    bool isOnline(QString xuid);
    int getPermissionLevel(QString xuid);
    void setPermissionLevelForUser(QString xuid, PermissionLevel level);
    QList<BedrockServer::ConfigEntry*> serverConfiguration();
    int maxPlayers();
    int pendingShutdownSeconds();
    void abortPendingShutdown();
signals:
    void serverStateChanged(BedrockServer::ServerState newState);

    void serverOutput(BedrockServer::OutputType type, QString outputLine);
    void backupStarting();   // A request has been made to start a backup
    void backupStarted();    // A command has been sent to start the backup and the server has started it
    void backupInProgres();  // The server is still doing stuff
    void backupSavingData(); // We are now saving data from the servers file
    void backupFinishedOnServer(); // We have told the server we have finished the backup, and the server has responded.
    void backupFailed(); // The backup failed for some reason.
    void backupFinished(QString zipFilePath); // The backup has been complete and the zip file is ready.
    void backupComplete(); // All done. (The temp file is deleted after this).

    void playerConnected(QString name, QString xuid);
    void playerDisconnected(QString name, QString xuid);

    void serverDifficulty(ServerDifficulty difficulty);
    void serverPermissionList(QStringList ops, QStringList members, QStringList visitors);
    void serverPermissionsChanged(); // Connect to this if you care about permission changes.

    void serverWhitelist(QStringList members);

    void serverStatusLine(QString status); // A one line server status summary.

    void serverConfigurationUpdated();
    void shutdownServerIn(int seconds);

public slots:
    void scheduleBackup(); // Starts a backup unless one has run too recently, if so, schedules it instead.
    void startBackup(); // You must call completeBackup if you get a backupFinished. (Once you've done with the zip)
    void completeBackup();
    void startServer();
    void startServerAfter(int ms);
    void restartServerAfter(int ms);
    void stopServer();
    void stopAndRestartServer();
    void sendCommandToServer(QString command);
    void setDifficulty(int difficulty);
    void saveConfiguration();
//    void StopServer();

private:
    enum ConfigValueType { String,Integer,Float,Boolean };

    BedrockServerModel *model;
    QStandardItem *onlinePlayers;
    QStandardItem *operators;
    ServerDifficulty difficulty;
    QStringList responseBuffer;
    QList<ConfigEntry*> serverConfig;
    QMap<QString,ConfigEntry*> serverConfigByName;

    QString serverRootFolder;
    QProcess *serverProcess;
    QByteArray processOutputBuffer;
    QTemporaryDir *tempDir;
    ServerState state;
    QTimer startTimer;
    QTimer shutdownPendingTimer;
    QTimer shutdownHeartbeatTimer;
    QTimer backupDelayTimer; // if running
    int backupDelaySeconds; // Automated or triggered backups will wait at least this seconds between firing.
    int maximumPlayerCount; // Read from config.
    bool backupScheduled; // set true to do a backup
    bool restartOnServerExit; // if true then unless the stop server method has been called the server will be kept running.

    void actuallyStartServer(); // Does the server startup
    QString readServerLine();
    QString cleanServerLine(QString line);
    bool canReadServerLine();
    bool serverHasData();
    void processRunningBackup();
    void processFinishedBackup();
    void parseOutputForEvents(QString output);
    QPair<QString,QString> parsePlayerString(QString playerString);
    void setState(ServerState newState);
    bool serverRootIsValid();
    void processResponseBuffer();
    void emitStatusLine();
    void loadConfiguration();
    ConfigValueType getTypeOfConfigValue(QString name);
    QStringList getPossibleValues(QString name);
    QVariant getConfigValue(QString name);
    bool restartAfterStopped;

private slots:
    void handleServerOutput();
    void handleZipComplete();
};

#endif // BEDROCKSERVER_H

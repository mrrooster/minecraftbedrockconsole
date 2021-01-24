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

class BedrockServer : public QObject
{
    Q_OBJECT
public:
    explicit BedrockServer(QObject *parent = nullptr);

    enum OutputType { ServerInfoOutput,ServerErrorOutput,ServerStatus,InfoOutput,ErrorOutput };
    enum ServerState { ServerLoading,ServerStartup,ServerRunning,ServerNotRunning,ServerStopped,ServerShutdown,ServerRestarting };

    QString GetCurrentStateName();
    ServerState GetCurrentState();
    QString stateName(ServerState state);
    void setServerRootFolder(QString folder);
    void setBackupDelaySeconds(int seconds);

signals:
    void serverStateChanged(ServerState newState);

    void serverOutput(OutputType type, QString outputLine);
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

public slots:
    void scheduleBackup(); // Starts a backup unless one has run too recently, if so, schedules it instead.
    void startBackup(); // You must call completeBackup if you get a backupFinished. (Once you've done with the zip)
    void completeBackup();
    void startServer();
    void stopServer();
    void sendCommandToServer(QString command);
//    void StopServer();

private:
    QString serverRootFolder;
    QProcess *serverProcess;
    QByteArray processOutputBuffer;
    QTemporaryDir *tempDir;
    QMap<QString,QString> knownPlayers;
    ServerState state;
    QTimer backupDelayTimer; // if running
    int backupDelaySeconds; // Automated or triggered backups will wait at least this seconds between firing.
    bool backupScheduled; // set true to do a backup
    bool restartOnServerExit; // if true then unless the stop server method has been called the server will be kept running.

    QString readServerLine();
    bool canReadServerLine();
    bool serverHasData();
    void processRunningBackup();
    void processFinishedBackup();
    void parseOutputForEvents(QString output);
    QPair<QString,QString> parsePlayerString(QString playerString);
    void setState(ServerState newState);
    bool serverRootIsValid();

private slots:
    void handleServerOutput();
    void handleZipComplete();
};

#endif // BEDROCKSERVER_H

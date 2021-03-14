#ifndef BEDROCKSERVERMODEL_H
#define BEDROCKSERVERMODEL_H

#include <QAbstractItemModel>
#include <server/bedrockserver.h>
#include <QList>

class BedrockServerModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit BedrockServerModel(BedrockServer *parent = nullptr);
    ~BedrockServerModel();

    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QString getXuidFromIndex(QModelIndex index);
    QString getPlayerNameFromXuid(QString xuid);
    bool isOnline(QString xuid);
    int getPermissionLevel(QString xuid);
    int onlinePlayerCount();
    int onlineOpCount();
signals:
    void serverPermissionsChanged(); // Connect to this if you care about permission changes.
private:
    BedrockServer *server;
    QList<QString> onlineUsers;
    QList<QString> opsByXuid;
    QList<QString> membersByXuid;
    QList<QString> visitorsByXuid;
    QMap<QString,QPair<QString,QDateTime>> xuidToGamertag; // gamertag, <xuid,last seen time>
    QModelIndex onlineRoot;
    QModelIndex opsRoot;
    QModelIndex membersRoot;
    QModelIndex visitorsRoot;

    QString xuidToName(QString xuid) const;
    void saveUserMappings();
    void loadUserMappings();
};

#endif // BEDROCKSERVERMODEL_H

#include "bedrockservermodel.h"
#include <QFileIconProvider>
#include <QModelIndex>
#include <QSettings>
#include <QDateTime>
#include <QAbstractFileIconProvider>

#define ONLINE_ROOT 1
#define ONLINE_USER 2
#define OPS_ROOT 3
#define OPS_USER 4
#define MEMBERS_ROOT 5
#define MEMBERS_USER 6
#define VISITORS_ROOT 7
#define VISITORS_USER 8

BedrockServerModel::BedrockServerModel(BedrockServer *parent)
    : QAbstractItemModel(parent),server(parent)
{
    //this->operators->appendRow(new QStandardItem(QFileIconProvider().icon(QAbstractFileIconProvider::Computer), tr("APerson")));
    //this->operators->appendRow(new QStandardItem(QFileIconProvider().icon(QAbstractFileIconProvider::Computer), tr("AnotherPerson")));

    this->onlineRoot = createIndex(0,0,(quintptr)ONLINE_ROOT);
    this->opsRoot = createIndex(2,0,(quintptr)OPS_ROOT);
    this->membersRoot = createIndex(1,0,(quintptr)MEMBERS_ROOT);
    this->visitorsRoot = createIndex(3,0,(quintptr)VISITORS_ROOT);

    connect(this->server,&BedrockServer::serverStateChanged,this,[=](BedrockServer::ServerState state) {
       if (state==BedrockServer::ServerRunning) {
           this->server->sendCommandToServer("permission list");
       } else {
           if (!this->onlineUsers.isEmpty()) {
               this->beginRemoveRows(this->onlineRoot,0,this->onlineUsers.size()-1);
               this->onlineUsers.clear();
               this->endRemoveRows();
           }
       }
    });
    connect(this->server,&BedrockServer::playerConnected,this,[=](QString name, QString xuid) {
       if (!this->onlineUsers.contains(xuid)) {
           this->beginInsertRows(this->onlineRoot,this->onlineUsers.size(),this->onlineUsers.size());
           this->onlineUsers.append(xuid);
           this->endInsertRows();
       }
       if (!this->xuidToGamertag.contains(xuid)) {
           this->xuidToGamertag.insert(xuid,QPair<QString,QDateTime>(name,QDateTime::currentDateTimeUtc()));
       }
    });
    connect(this->server,&BedrockServer::playerDisconnected,this,[=](QString name, QString xuid) {
       if (this->onlineUsers.contains(xuid)) {
           int idx = this->onlineUsers.indexOf(xuid);
           this->beginRemoveRows(this->onlineRoot,idx,idx);
           this->onlineUsers.removeOne(xuid);
           this->endRemoveRows();
       }
       if (this->xuidToGamertag.contains(xuid)) {
           this->xuidToGamertag.remove(xuid);
       }
       this->xuidToGamertag.insert(xuid,QPair<QString,QDateTime>(name,QDateTime::currentDateTimeUtc()));
    });
    connect(this->server,&BedrockServer::serverPermissionList,this,[=](QStringList ops, QStringList members, QStringList visitors) {
        // Handle updated permissions
        if (!this->opsByXuid.isEmpty()) {
            this->beginRemoveRows(this->opsRoot,0,this->opsByXuid.size()-1);
            this->opsByXuid.clear();
            this->endRemoveRows();
        }
        if (!ops.isEmpty()) {
            this->beginInsertRows(this->opsRoot,0,ops.size()-1);
            this->opsByXuid.append(ops);
            this->endInsertRows();
        }

        if (!this->membersByXuid.isEmpty()) {
            this->beginRemoveRows(this->membersRoot,0,this->membersByXuid.size()-1);
            this->membersByXuid.clear();
            this->endRemoveRows();
        }
        if (!members.isEmpty()) {
            this->beginInsertRows(this->membersRoot,0,members.size()-1);
            this->membersByXuid.append(members);
            this->endInsertRows();
        }

        if (!this->visitorsByXuid.isEmpty()) {
            this->beginRemoveRows(this->visitorsRoot,0,this->visitorsByXuid.size()-1);
            this->visitorsByXuid.clear();
            this->endRemoveRows();
        }
        if (!visitors.isEmpty()) {
            this->beginInsertRows(this->visitorsRoot,0,visitors.size()-1);
            this->visitorsByXuid.append(visitors);
            this->endInsertRows();
        }
        emit this->serverPermissionsChanged();
    });
    loadUserMappings();
}

BedrockServerModel::~BedrockServerModel()
{
    saveUserMappings();
}

QVariant BedrockServerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return QVariant();
}

QModelIndex BedrockServerModel::index(int row, int column, const QModelIndex &parent) const
{
    // FIXME: Implement me!
    if (!parent.isValid()) {
        switch (row) {
        case 0 : return this->onlineRoot;
        case 1 : return this->membersRoot;
        case 2 : return this->opsRoot;
        case 3 : return this->visitorsRoot;
        }
    } else if (parent.internalId()==ONLINE_ROOT) {
        return createIndex(row,column,(quintptr)ONLINE_USER);
    } else if (parent.internalId()==MEMBERS_ROOT) {
        return createIndex(row,column,(quintptr)MEMBERS_USER);
    } else if (parent.internalId()==OPS_ROOT) {
        return createIndex(row,column,(quintptr)OPS_USER);
    } else if (parent.internalId()==VISITORS_ROOT) {
        return createIndex(row,column,(quintptr)VISITORS_USER);
    }
}

QModelIndex BedrockServerModel::parent(const QModelIndex &index) const
{
    switch(index.internalId()) {
    case ONLINE_USER : return this->onlineRoot;
    case MEMBERS_USER : return this->membersRoot;
    case OPS_USER : return this->opsRoot;
    case VISITORS_USER : return this->visitorsRoot;
    }
    return QModelIndex();
}

int BedrockServerModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return 4;

    switch(parent.internalId()) {
    case ONLINE_ROOT : return this->onlineUsers.count();
    case MEMBERS_ROOT : return this->membersByXuid.count();
    case OPS_ROOT : return this->opsByXuid.count();
    case VISITORS_ROOT : return this->visitorsByXuid.count();
    }
    return 0;
}

int BedrockServerModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return 1;
    return 1;
    // FIXME: Implement me!
}

QVariant BedrockServerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.internalId()==ONLINE_ROOT) {
        return (role==Qt::DisplayRole) ? QVariant("Online players") :
               (role==Qt::DecorationRole) ? QFileIconProvider().icon(QAbstractFileIconProvider::Folder) :
               QVariant() ;
    } else if (index.internalId()==MEMBERS_ROOT) {
            return (role==Qt::DisplayRole) ? QVariant("Members") :
                   (role==Qt::DecorationRole) ? QFileIconProvider().icon(QAbstractFileIconProvider::Folder) :
                   QVariant() ;
    } else if (index.internalId()==OPS_ROOT) {
            return (role==Qt::DisplayRole) ? QVariant("Operators") :
                   (role==Qt::DecorationRole) ? QFileIconProvider().icon(QAbstractFileIconProvider::Folder) :
                   QVariant() ;
    } else if (index.internalId()==VISITORS_ROOT) {
            return (role==Qt::DisplayRole) ? QVariant("Visitors") :
                   (role==Qt::DecorationRole) ? QFileIconProvider().icon(QAbstractFileIconProvider::Folder) :
                   QVariant() ;
    } else if (index.parent().isValid() && index.parent().internalId()==ONLINE_ROOT) {
        return (role==Qt::DisplayRole) ? QVariant(this->xuidToName(this->onlineUsers.at(index.row()))) :
                                         (role==Qt::DecorationRole) ? QFileIconProvider().icon(QAbstractFileIconProvider::Computer) :
                                         QVariant() ;
    } else if (index.parent().isValid() && index.parent().internalId()==MEMBERS_ROOT) {
        return (role==Qt::DisplayRole) ? QVariant(this->xuidToName(this->membersByXuid.at(index.row()))) :
                                         (role==Qt::DecorationRole) ? QFileIconProvider().icon(QAbstractFileIconProvider::Computer) :
                                         QVariant() ;
    } else if (index.parent().isValid() && index.parent().internalId()==OPS_ROOT) {
        return (role==Qt::DisplayRole) ? QVariant(this->xuidToName(this->opsByXuid.at(index.row()))) :
                                         (role==Qt::DecorationRole) ? QFileIconProvider().icon(QAbstractFileIconProvider::Computer) :
                                         QVariant() ;
    } else if (index.parent().isValid() && index.parent().internalId()==VISITORS_ROOT) {
        return (role==Qt::DisplayRole) ? QVariant(this->xuidToName(this->visitorsByXuid.at(index.row()))) :
                                         (role==Qt::DecorationRole) ? QFileIconProvider().icon(QAbstractFileIconProvider::Computer) :
                                         QVariant() ;
    }

    return QVariant();
}

QString BedrockServerModel::getXuidFromIndex(QModelIndex index)
{
    switch(index.internalId()) {
    case MEMBERS_USER : return this->membersByXuid.at(index.row());
    case OPS_USER : return this->opsByXuid.at(index.row());
    case VISITORS_USER : return this->visitorsByXuid.at(index.row());
    case ONLINE_USER : return this->onlineUsers.at(index.row());
    }
    return "";
}

QString BedrockServerModel::getPlayerNameFromXuid(QString xuid)
{
    if (this->xuidToGamertag.contains(xuid)) {
        return this->xuidToGamertag.value(xuid).first;
    }
    return xuid;
}

bool BedrockServerModel::isOnline(QString xuid)
{
    return this->onlineUsers.contains(xuid);
}

int BedrockServerModel::getPermissionLevel(QString xuid)
{
    if (this->membersByXuid.contains(xuid)) {
        return 0;
    } else if (this->opsByXuid.contains(xuid)) {
        return 1;
    } else if (this->visitorsByXuid.contains(xuid)) {
        return 2;
    }
    return -1;
}

int BedrockServerModel::onlinePlayerCount()
{
    return this->onlineUsers.size();
}

int BedrockServerModel::onlineOpCount()
{
    int count=0;
    for(auto x=this->onlineUsers.constBegin();x!=this->onlineUsers.constEnd();x++) {
        QString user = (*x);
        if (getPermissionLevel(user)==BedrockServer::Operator) {
            count++;
        }
    }
    return count;
}

QString BedrockServerModel::xuidToName(QString xuid) const
{
    if (this->xuidToGamertag.contains(xuid)) {
        return this->xuidToGamertag.value(xuid).first;
    }
    return xuid;
}

void BedrockServerModel::saveUserMappings()
{
    QSettings opts;

    opts.beginGroup("users");
    opts.beginWriteArray("mappings");

    int idx = 0;
    for (QMap<QString,QPair<QString,QDateTime>>::const_key_value_iterator i = this->xuidToGamertag.constKeyValueBegin(); i!=this->xuidToGamertag.constKeyValueEnd(); i++) {
        QString xuid = (*i).first;
        QString name = (*i).second.first;
        QDateTime lastSeen = isOnline(xuid) ? QDateTime::currentDateTimeUtc() : (*i).second.second;
        opts.setArrayIndex(idx++);
        opts.setValue("xuid",xuid);
        opts.setValue("name",name);
        opts.setValue("lastSeen",lastSeen);

    }
    opts.endArray();
    opts.endGroup();
}

void BedrockServerModel::loadUserMappings()
{
    QSettings opts;

    opts.beginGroup("users");
    int size = opts.beginReadArray("mappings");
    for(int x=0;x<size;x++) {
        opts.setArrayIndex(x);
        QString name = opts.value("name").toString();
        QString xuid = opts.value("xuid").toString();
        QDateTime lastSeen = opts.value("lastSeen").toDateTime();

        if (lastSeen.addMonths(6) >= QDateTime::currentDateTimeUtc()) {
            this->xuidToGamertag.insert(xuid,QPair<QString,QDateTime>(name,lastSeen));
        }
    }
}

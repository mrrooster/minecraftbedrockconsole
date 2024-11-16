#include "onlineplayerwidget.h"
#include <QLabel>
#include <server/bedrockservermodel.h>

OnlinePlayerWidget::OnlinePlayerWidget(QWidget *parent) :QWidget(parent), server(nullptr)
{
    QLabel *usersOnline = new QLabel("",this);
    usersOnline->setStyleSheet("background-color: #fff7e0;");
    usersOnline->setGeometry(geometry());
    this->usersOnline = usersOnline;

    QLabel *opsOnline = new QLabel("",this);
    opsOnline->setStyleSheet("background-color: #c6d8f5;");
    opsOnline->setGeometry(geometry());
    this->opsOnline = opsOnline;

    QLabel *overlay = new QLabel("",this);
    overlay->setAlignment(Qt::AlignCenter);
    overlay->setGeometry(geometry());
    this->overlay = overlay;
}

void OnlinePlayerWidget::setServer(BedrockServer *server)
{
    this->server = server;

    connect(server,&BedrockServer::playerConnected,this,[=](QString,QString) {
       setup();
    });
    connect(server,&BedrockServer::playerDisconnected,this,[=](QString,QString) {
       setup();
    });
    connect(server,&BedrockServer::serverStateChanged,this,[=](BedrockServer::ServerState) {
        setup();
    });
    setup();
}

void OnlinePlayerWidget::resizeEvent(QResizeEvent *event)
{
    this->usersOnline->setGeometry(geometry());
    this->overlay->setGeometry(geometry());
    setup();
    QWidget::resizeEvent(event);
}

void OnlinePlayerWidget::setup()
{
    int onlineCount = 0;
    int onlineOpCount = 0;
    int totalCount = 0;
    int width = (totalCount==0) ? 0 : this->width() / totalCount;
    if (this->server) {
        BedrockServerModel *model = (BedrockServerModel*)this->server->getServerModel();
        QString overlayText;
        onlineCount = model->onlinePlayerCount();
        onlineOpCount = model->onlineOpCount();
        totalCount = this->server->maxPlayers();
        width = (totalCount==0) ? 0 : this->width() / totalCount;
        if (onlineOpCount>0) {
            overlayText = tr("Online: %1 of %2, %3 ops.")
                                    .arg(QString::number(onlineCount))
                                    .arg(QString::number(totalCount))
                                    .arg(QString::number(onlineOpCount));
        } else {
            overlayText = tr("Online: %1 of %2")
                                    .arg(QString::number(onlineCount))
                                    .arg(QString::number(totalCount));

        }

        switch (this->server->GetCurrentState()) {
        case BedrockServer::ServerNotRunning:
        case BedrockServer::ServerStopped:
            overlayText = tr("Server is not running");
            break;
        case BedrockServer::ServerStartup:
        case BedrockServer::ServerLoading:
        case BedrockServer::ServerRestarting:
            overlayText = tr("Server is starting up");
            break;
        case BedrockServer::ServerShutdown:
            overlayText = tr("Server is shutting down");
            break;
        }
        this->overlay->setText(overlayText);
    }
    //this->usersOnline->setMaximumWidth(onlineCount * width);
    //this->usersOnline->setMinimumWidth(onlineCount * width);
    if (totalCount>0) {
        QRect geom = geometry();
        geom.setWidth((geom.width() / totalCount) * onlineOpCount);
        this->opsOnline->setGeometry(geom);
        this->opsOnline->setAutoFillBackground(true);

        this->usersOnline->setGeometry(geom);
        geom = geometry();
        geom.setWidth((geom.width() / totalCount) * onlineCount);
        this->usersOnline->setGeometry(geom);
        this->usersOnline->setAutoFillBackground(true);
    }

    this->usersOnline->repaint();
    this->opsOnline->repaint();
    this->repaint();
}

#include "playerinfowidget.h"
#include "ui_playerinfowidget.h"

PlayerInfoWidget::PlayerInfoWidget(BedrockServer *server, QString xuid, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PlayerInfoWidget),
    server(server),
    xuid(xuid)
{
    ui->setupUi(this);

    this->ui->xuid->setText(xuid);
    //this->ui->name->setStyleSheet("font-weight: bold; font-size: 18pt;");
    this->ui->name->setText(this->server->getPlayerNameFromXuid(xuid));

    setOnline(this->server->isOnline(xuid));
    this->ui->permissionLevel->setCurrentIndex(this->server->getPermissionLevel(xuid));

    connect(this->ui->permissionLevel,&QComboBox::currentIndexChanged,this,[=](int idx) {
        this->server->setPermissionLevelForUser(this->xuid,(BedrockServer::PermissionLevel) idx);
    });

    connect(this->server,&BedrockServer::serverPermissionsChanged,this,[=]() {
        this->ui->permissionLevel->setCurrentIndex(this->server->getPermissionLevel(xuid));
    });

    connect(this->server,&BedrockServer::playerConnected,this,[=](QString name,QString xuid){
        if (this->xuid==xuid) {
            setOnline(true);
            this->ui->name->setText(name);
        }
    });
    connect(this->server,&BedrockServer::playerDisconnected,this,[=](QString ,QString xuid){
        if (this->xuid==xuid) {
            setOnline(false);
        }
    });
}

PlayerInfoWidget::~PlayerInfoWidget()
{
    delete ui;
}

void PlayerInfoWidget::setOnline(bool state)
{
    this->ui->onlineStatus->setText( state ? "Online" : "Offline" );
    this->ui->onlineStatus->setStyleSheet( state ? "color: green;" : "color: red;" );
}

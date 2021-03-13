#include "playerinfowidget.h"
#include "ui_playerinfowidget.h"
#include <QInputDialog>
#include <QDebug>

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

    connect(this->ui->kickUser,&QPushButton::clicked,this,[=]() {
        if (this->userIsOnline) {
            bool ok;
            QString reason = QInputDialog::getText(this,QString(tr("Kick player: %1")).arg(this->ui->name->text()),QString(tr("Click OK to kick <b>%1</b>, otherwise click Cancel. You can also enter a reason.")).arg(this->ui->name->text()),QLineEdit::Normal,"",&ok);
            if (ok) {
                qDebug()<<"Kicking"<<this->ui->name->text()<<"for reason:"<<reason;
                this->server->sendCommandToServer(QString("kick %1 %2").arg(this->server->getPlayerNameFromXuid(this->xuid)).arg(reason));
            }
        }
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
    this->userIsOnline=state;
    this->ui->onlinePanel->setVisible(state);
}

#include "serverconsolewidget.h"
#include "ui_serverconsolewidget.h"

ServerConsoleWidget::ServerConsoleWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ServerConsoleWidget)
{
    ui->setupUi(this);
    this->ui->serverOutput->setMaximumBlockCount(20000);
}

ServerConsoleWidget::~ServerConsoleWidget()
{
    delete ui;
}

void ServerConsoleWidget::setServer(BedrockServer *server)
{
    this->server = server;
    connect(this->server,&BedrockServer::serverOutput,this,&ServerConsoleWidget::handleServerOutput);
    connect(this->server,&BedrockServer::serverStateChanged,this,&ServerConsoleWidget::handleServerStateChange);
    connect(ui->sendButton,&QPushButton::clicked,this,[=]() {
        this->server->sendCommandToServer(this->ui->commandEdit->text());
        this->ui->commandEdit->clear();
    });
    this->ui->commandEdit->setPlaceholderText(tr("Server command..."));
}

void ServerConsoleWidget::handleServerOutput(BedrockServer::OutputType type, QString message)
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
    case BedrockServer::OutputType::WarningOutput :
       this->ui->serverOutput->appendHtml(QString("<span style='background-color: #e90b00; color: yellow;'><code>%1</code></span>").arg(message));
       break;
    case BedrockServer::OutputType::ServerStatus :
       this->ui->serverOutput->appendHtml(QString("<span style='color: #31ac0f;'><code>%1</code></span>").arg(message));
       break;
    default:
        this->ui->serverOutput->appendPlainText(message);
    }
    this->ui->serverOutput->ensureCursorVisible();
}

void ServerConsoleWidget::handleServerStateChange(BedrockServer::ServerState newState)
{

}

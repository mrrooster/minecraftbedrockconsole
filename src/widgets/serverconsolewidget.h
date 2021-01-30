#ifndef SERVERCONSOLEWIDGET_H
#define SERVERCONSOLEWIDGET_H

#include <QWidget>
#include <server/bedrockserver.h>

namespace Ui {
class ServerConsoleWidget;
}

class ServerConsoleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ServerConsoleWidget(QWidget *parent = nullptr);
    ~ServerConsoleWidget();

    void setServer(BedrockServer *server);
private:
    Ui::ServerConsoleWidget *ui;
    BedrockServer *server;
private slots:
    void handleServerOutput(BedrockServer::OutputType type, QString message);
    void handleServerStateChange(BedrockServer::ServerState newState);
};

#endif // SERVERCONSOLEWIDGET_H

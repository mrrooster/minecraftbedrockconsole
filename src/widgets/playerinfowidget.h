#ifndef PLAYERINFOWIDGET_H
#define PLAYERINFOWIDGET_H

#include <QWidget>
#include <server/bedrockserver.h>

namespace Ui {
class PlayerInfoWidget;
}

class PlayerInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlayerInfoWidget(BedrockServer *server, QString xuid, QWidget *parent = nullptr);
    ~PlayerInfoWidget();

private:
    Ui::PlayerInfoWidget *ui;
    BedrockServer *server;
    QString xuid;
    bool userIsOnline;

    void setOnline(bool state);
};

#endif // PLAYERINFOWIDGET_H

#ifndef ONLINEPLAYERWIDGET_H
#define ONLINEPLAYERWIDGET_H

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <server/bedrockserver.h>

class OnlinePlayerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OnlinePlayerWidget(QWidget *parent = nullptr);
    void setServer(BedrockServer *server);

signals:

protected:
    virtual void resizeEvent(QResizeEvent *event);
private:
    BedrockServer *server;
    QLabel *usersOnline;
    QLabel *opsOnline;
    QLabel *overlay;

    void setup();
};

#endif // ONLINEPLAYERWIDGET_H

#ifndef ONLINEPLAYERBAR_H
#define ONLINEPLAYERBAR_H

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <server/bedrockserver.h>

class OnlinePlayerBar : public QWidget
{
    Q_OBJECT
public:
    explicit OnlinePlayerBar(QWidget *parent = nullptr);
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

#endif // ONLINEPLAYERBAR_H

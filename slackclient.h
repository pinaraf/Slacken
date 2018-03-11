#ifndef SLACKCLIENT_H
#define SLACKCLIENT_H

#include <QObject>
#include <QtNetworkAuth>

class SlackClient : public QObject
{
    Q_OBJECT
public:
    explicit SlackClient(QObject *parent = nullptr);
    void fire();
signals:

public slots:

private:
    QOAuth2AuthorizationCodeFlow oauth2;
};

#endif // SLACKCLIENT_H

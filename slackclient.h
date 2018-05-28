#ifndef SLACKCLIENT_H
#define SLACKCLIENT_H

#include <QObject>
#include <QtNetworkAuth>
#include <QMap>

class QWebSocket;

class SlackClient;

class SlackChannel
{
    Q_GADGET
public:
    SlackChannel(SlackClient *client, const QJsonValueRef &source);

//private:
    QDateTime created;
    QString id;
    QString name;
    bool is_group;
    bool is_im;
    bool is_channel;
    bool is_mpim;
    bool is_general;
    bool is_archived;
    bool is_member;
};

class SlackUser
{
    Q_GADGET
public:
    SlackUser(const QJsonValueRef &source);

//private:
    QString id;
    QString name;
    bool is_deleted;
};

class SlackMessage
{
    Q_GADGET
public:
    SlackMessage(const QJsonObject &source);
    SlackMessage(const QString &user, const QString &msg);

    QString type;
    QString user;
    QString text;
    QDateTime when;
};

class SlackClient : public QObject
{
    Q_OBJECT
public:
    explicit SlackClient(QObject *parent = nullptr);
    void fire();

    QList<SlackChannel> channels() const;
    QList<SlackUser> users() const;

    const SlackUser &user(const QString &id) const;

    void requestHistory(const QString &id);

    void sendMessage(const QString &channel, const QString &msg);
signals:
    void authenticated();
    void channelAdded(const SlackChannel &channel);
    void userAdded(const SlackUser &user);

    void channelHistory(const QList<SlackMessage> &history);

    void newMessage(const QString &channel, const SlackMessage &msg);
public slots:
    void login();

private:
    QString selfId;
    QWebSocket *chaussette;
    int socketMessageId;
    QOAuth2AuthorizationCodeFlow oauth2;
    QMap<QString, SlackChannel> m_channels;
    QMap<QString, SlackUser> m_users;
};

#endif // SLACKCLIENT_H

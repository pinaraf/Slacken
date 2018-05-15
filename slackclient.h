#ifndef SLACKCLIENT_H
#define SLACKCLIENT_H

#include <QObject>
#include <QtNetworkAuth>
#include <QMap>

class SlackConversation
{
    Q_GADGET
public:
    SlackConversation(const QJsonValueRef &source);

//private:
    QString id;
    QString name;
    bool is_channel;
    bool is_group;
    bool is_im;
    bool is_general;
    bool is_archived;
};

class SlackClient : public QObject
{
    Q_OBJECT
public:
    explicit SlackClient(QObject *parent = nullptr);
    void fire();

    QList<SlackConversation> conversations() const;

signals:
    void authenticated();

public slots:
    void login();

private:
    void listConversations(const QString &cursor = "");

    QOAuth2AuthorizationCodeFlow oauth2;
    QMap<QString, SlackConversation> m_conversations;
};

#endif // SLACKCLIENT_H

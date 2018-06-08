/**
    This file is part of Slacken.

    Slacken is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Slacken is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Slacken.  If not, see <http://www.gnu.org/licenses/>.
**/

#ifndef SLACKCLIENT_H
#define SLACKCLIENT_H

#include <QObject>
#include <QtNetworkAuth>
#include <QColor>
#include <QMap>
#include <vector>

class QWebSocket;

class SlackClient;
class SlackChannel;
class SlackUser;
class SlackMessage;

class SlackChannel : public QObject
{
    Q_OBJECT
public:
    SlackChannel(SlackClient *client, const QJsonValue &source);

    QDateTime created;
    QDateTime last_read;
    QString id;
    QString name;
    QString topic;
    bool has_unread;
    bool is_group;
    bool is_im;
    bool is_channel;
    bool is_mpim;
    bool is_general;
    bool is_archived;
    bool is_member;

public slots:
    void setHasUnread(bool unread);
    void markRead(const SlackMessage &lastMessage);
signals:
    void unreadChanged();

};

class SlackUser
{
    Q_GADGET
public:
    SlackUser(const QJsonValueRef &source);

    QString id;
    QString name;
    bool is_deleted;
};

class SlackMessageAttachment
{
    Q_GADGET
public:
    SlackMessageAttachment(const QJsonObject &source);

    QColor color;
    QString text;
    QString fallback;
    QString title;
};

class SlackMessage
{
    Q_GADGET
public:
    SlackMessage(const QJsonObject &source);
    SlackMessage(const QString &user, const QString &msg);

    QString type;
    QString user;
    QString username;   // Seriously slack ?
    QString text;
    QDateTime when;
    QString ts;

    std::vector<SlackMessageAttachment> attachments;
};

class SlackClient : public QObject
{
    Q_OBJECT
public:
    explicit SlackClient(QObject *parent = nullptr);
    void start();

    const std::map<QString, SlackChannel*> &channels() const;
    const std::map<QString, SlackUser> &users() const;

    bool hasUser(const QString &id) const;
    const SlackUser &user(const QString &id) const;
    QString userId(const QString &userName) const;
    QString currentUserId() const;

    void requestHistory(const QString &id);

    void sendMessage(const QString &channel, const QString &msg);

    QString teamName() const;
    QString currentToken() const;

    // Todo : channelType as enum.
    void markChannelRead(const QString &channelType, const QString &channel, const QString &lastTimestamp);

    void openConversation(const QStringList &users);
    void openConversation(const QString &channel);
signals:
    void authenticated();
    void hasBasicData();
    void channelAdded(SlackChannel *channel);
    void userAdded(const SlackUser &user);

    void channelHistory(const QList<SlackMessage> &history);

    void newMessage(const QString &channel, const SlackMessage &msg);

    void desktopNotification(const QString &title, const QString &subtitle, const QString &message);

    void channelJoined(SlackChannel *channel);
    void channelLeft(SlackChannel *channel);

public slots:
    void login(const QString &existingToken);

private:
    void fetchCounts();

    QString m_teamName;
    QString selfId;
    QUrl webSocketUrl;
    QDateTime lastPong;
    QWebSocket *chaussette;
    int socketMessageId;
    QOAuth2AuthorizationCodeFlow oauth2;
    std::map<QString, SlackChannel*> m_channels;
    std::map<QString, SlackUser> m_users;
};

#endif // SLACKCLIENT_H

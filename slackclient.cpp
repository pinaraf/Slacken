#include "slackclient.h"
#include <QDebug>
#include <QOAuthHttpServerReplyHandler>
#include <QDesktopServices>
#include <QWebSocket>

static const char *clientId = "3457590858.328048934181";
static const char *clientSecret = "673a852137cc6a09717d39c480c3d88f";
//static const char *verificationToken = "wOYGN5tkgAYH69WK963i0eO4";

SlackClient::SlackClient(QObject *parent) : QObject(parent)
{
    auto replyHandler = new QOAuthHttpServerReplyHandler(1337, this);
    oauth2.setClientIdentifier(clientId);
    oauth2.setClientIdentifierSharedKey(clientSecret);
    oauth2.setReplyHandler(replyHandler);
    oauth2.setAuthorizationUrl(QUrl("https://slack.com/oauth/authorize"));
    oauth2.setAccessTokenUrl(QUrl("https://slack.com/api/oauth.access"));
    //oauth2.setScope("teams:read users:read teams:write files:read files:write rtm:stream client read post");
    oauth2.setScope("client");

    connect(&oauth2, &QOAuth2AuthorizationCodeFlow::statusChanged, [=](
            QAbstractOAuth::Status status) {
        qDebug() << "Status :" << static_cast<int> (status);
        if (status == QAbstractOAuth::Status::Granted) {
            qDebug() << "Authenticated !";
            emit authenticated();
        }
    });
    oauth2.setModifyParametersFunction([&](QAbstractOAuth::Stage stage, QVariantMap *parameters) {
        qDebug() << "I'm lost here : stage = " << static_cast<int> (stage);
        qDebug() << (*parameters);
        if (stage == QAbstractOAuth::Stage::RequestingAuthorization)
            parameters->insert("duration", "permanent");
    });
    connect(&oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, [](const QUrl &url) {
        qDebug() << "Open " << url;
        QDesktopServices::openUrl(url);
    });
}

void SlackClient::login(const QString &existingToken) {
    if (existingToken.isEmpty()) {
        oauth2.grant();
    } else {
        // Look me ma, I'm a hack
        oauth2.setToken(existingToken);
        emit authenticated();
    }
}

QString SlackClient::currentToken() const {
    return oauth2.token();
}

QString SlackClient::teamName() const {
    return m_teamName;
}

void SlackClient::fetchCounts() {
    QVariantMap query;
    query.insert("simple_unreads", true);
    auto replyCount = oauth2.post(QUrl("https://slack.com/api/users.counts"), query);
    connect(replyCount, &QNetworkReply::finished, [this, replyCount]() {
        auto doc = QJsonDocument::fromJson(replyCount->readAll());
        QFile f("/tmp/users.counts.json");
        f.open(QIODevice::WriteOnly);
        f.write(doc.toJson(QJsonDocument::Indented));

        auto setUnreads = [&] (const QString &listPath) {
            for (const QJsonValueRef channel: doc[listPath].toArray()) {
                auto &&channelObj = channel.toObject();
                auto it = m_channels.find(channelObj["id"].toString());
                if (it != m_channels.end()) {
                    it->second->setHasUnread(channelObj["has_unreads"].toBool());
                }
            }
        };

        setUnreads("channels");
        setUnreads("groups");
        setUnreads("ims");
    });
}

void SlackClient::start() {
    auto reply = oauth2.get(QUrl("https://slack.com/api/rtm.start"));
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        qDebug() << "Reply finished ! " << reply->errorString();

        this->fetchCounts();

        auto doc = QJsonDocument::fromJson(reply->readAll());

        selfId = doc["self"].toObject()["id"].toString();
        m_teamName = doc["team"].toObject()["name"].toString();

        emit(hasBasicData());
        // Instanciate each channel and user
        for (const QJsonValueRef user: doc["users"].toArray()) {
            emit userAdded(
                m_users.emplace(user.toObject()["id"].toString(), SlackUser(user)).first->second
            );
        }
        for (const QJsonValueRef channel: doc["channels"].toArray()) {
            emit channelAdded(
                m_channels.emplace(channel.toObject()["id"].toString(), new SlackChannel(this, channel)).first->second
            );
        }
        for (const QJsonValueRef channel: doc["groups"].toArray()) {
            emit channelAdded(
                m_channels.emplace(channel.toObject()["id"].toString(), new SlackChannel(this, channel)).first->second
            );
        }
        for (const QJsonValueRef channel: doc["ims"].toArray()) {
            emit channelAdded(
                m_channels.emplace(channel.toObject()["id"].toString(), new SlackChannel(this, channel)).first->second
            );
        }

        //qDebug() << doc.toJson(QJsonDocument::Indented);
        /*QFile f("/tmp/rtm.start.json");
        f.open(QIODevice::WriteOnly);
        f.write(doc.toJson(QJsonDocument::Indented));*/

        chaussette = new QWebSocket("", QWebSocketProtocol::VersionLatest, this);
        connect(chaussette, &QWebSocket::connected, []() {
            qDebug() << "Chaussette en ligne !";
        });
        connect(chaussette, &QWebSocket::disconnected, [this]() {
            qDebug() << "Chaussette hors ligne !";
            chaussette->open(webSocketUrl);
            fetchCounts();
        });
        connect(chaussette, &QWebSocket::textMessageReceived, [this](const QString &msg) {
            qDebug() << "Chaussette in : " << msg;
            auto doc = QJsonDocument::fromJson(msg.toUtf8());
            QString type = doc["type"].toString();
            if (type == "message") {
                // Mark the channel as having some unread things
                auto chanIt = m_channels.find(doc["channel"].toString());
                if (chanIt != m_channels.end()) {
                    chanIt->second->setHasUnread(true);
                }
                // And send to everybody the information
                emit(newMessage(doc["channel"].toString(), SlackMessage(doc.object())));
            } else if (type == "desktop_notification") {
                emit(desktopNotification(doc["title"].toString(), doc["subtitle"].toString(), doc["content"].toString()));
            } else if (type == "im_open" || type == "channel_joined" || type == "group_joined") {
                auto chanIt = m_channels.find(doc["channel"].toString());
                if (chanIt != m_channels.end()) {
                    emit(channelJoined(chanIt->second));
                }
            } else if (type == "im_close" || type == "channel_left" || type == "group_left") {
                auto chanIt = m_channels.find(doc["channel"].toString());
                if (chanIt != m_channels.end()) {
                    emit(channelLeft(chanIt->second));
                }
            } else if (type == "channel_created") {
                emit channelAdded(
                    m_channels.emplace(doc["id"].toString(), new SlackChannel(this, doc.object())).first->second
                );
            }
            /*if (doc["type"] == "hello")
                chaussette->sendTextMessage("{\"id\": 1, \"type\": \"message\", \"channel\": \"D4EPF2N22\", \"text\": \"LA CHAUSSETTE PARLE !\"}");*/
        });

        webSocketUrl = QUrl(doc["url"].toString());
        qDebug() << "Chaussette vers ... " << webSocketUrl;
        chaussette->open(webSocketUrl);
        QTimer *pingTimer = new QTimer(this);
        pingTimer->setInterval(5000);
        pingTimer->setSingleShot(false);
        pingTimer->start();
        connect(pingTimer, &QTimer::timeout, [this] () {
            qDebug() << "Ping ws...";
            chaussette->ping();
        });
        connect(chaussette, &QWebSocket::pong, [this] (quint64 elapsedTime, const QByteArray &) {
            qDebug() << "Pong " << elapsedTime;
        });
        socketMessageId = 0;
    });
}

bool SlackClient::hasUser(const QString &id) const
{
    return (m_users.find(id) != m_users.end());
}

const SlackUser & SlackClient::user(const QString &id) const
{
    return m_users.find(id)->second;
}

QString SlackClient::userId(const QString &nick) const
{
    for (auto &user: m_users) {
        if (user.second.name == nick)
            return user.second.id;
    }
    return QString();
}

const std::map<QString, SlackChannel*> &SlackClient::channels() const
{
    return m_channels;
}

void SlackClient::requestHistory(const QString &id)
{
    QString url = QString("https://slack.com/api/conversations.history?channel=%1&token=%2").arg(id).arg(oauth2.token());
    qDebug() << url;
    auto reply = oauth2.get(QUrl(url));
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        qDebug() << "History reply finished !";
        qDebug() << reply->errorString();
        QString whole_doc = reply->readAll();
        qDebug() << oauth2.state();

        auto doc = QJsonDocument::fromJson(whole_doc.toUtf8());

        QList<SlackMessage> messages;
        for (const QJsonValueRef msgJson: doc["messages"].toArray()) {
            messages << SlackMessage(msgJson.toObject());
        }
        qDebug() << "EMIT";
        emit(channelHistory(messages));
    });
}

void SlackClient::sendMessage(const QString &channel, const QString &msgText)
{
    QJsonObject msg;
    msg.insert("id", socketMessageId++);
    msg.insert("type", "message");
    msg.insert("channel", channel);
    msg.insert("text", msgText);
    chaussette->sendTextMessage(QString::fromUtf8(QJsonDocument(msg).toJson()));
    emit newMessage(channel, SlackMessage(selfId, msgText));
}

void SlackClient::markChannelRead(const QString &channelType, const QString &channel, const QString &lastTimestamp) {
    QVariantMap query;
    query.insert("channel", channel);
    query.insert("ts", lastTimestamp);
    QUrl url("https://slack.com/api/channels.mark");
    if (channelType == "im")
        url = QUrl("https://slack.com/api/im.mark");
    else if (channelType == "group")
        url = QUrl("https://slack.com/api/groups.mark");
    else if (channelType == "mpim")
        url = QUrl("https://slack.com/api/mpim.mark");
    qDebug() << url << query;
    auto reply = oauth2.post(url, query);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        qDebug() << "Mark channel reply finished !";
        qDebug() << reply->errorString();
        QString whole_doc = reply->readAll();
        qDebug() << whole_doc;
    });
}

SlackChannel::SlackChannel(SlackClient *client, const QJsonValue &sourceRef)
    : QObject(client)
{
    QJsonObject &&source = sourceRef.toObject();
    created     = QDateTime::fromTime_t(source["created"].toInt());
    id          = source["id"].toString();
    if (source.contains("is_im") && source["is_im"].toBool()) {
        // Type identification
        is_im = true;
        is_channel = false;
        is_group = false;

        is_member = source["is_open"].toBool();;
        name = client->user(source["user"].toString()).name;
    } else {
        // Type identification
        is_im       = false;
        is_channel  = source["is_channel"].toBool();
        is_mpim     = source["is_mpim"].toBool();

        name        = source["name"].toString();
        is_group    = source["is_group"].toBool();
        is_general  = source["is_general"].toBool();
        is_archived = source["is_archived"].toBool();
        if (is_group)
            is_member = source["is_open"].toBool();
        else
            is_member   = source["is_member"].toBool();
    }

    if (source.contains("topic")) {
        auto topicObject = source["topic"].toObject();
        topic = topicObject["value"].toString();
    }

    if (source.contains("last_read")) {
        last_read = QDateTime::fromMSecsSinceEpoch(source["last_read"].toString().toDouble() * 1000 + 1000);
    } else {
        last_read = QDateTime();
    }

    has_unread = false;
}

void SlackChannel::setHasUnread(bool unread) {
    if (unread != has_unread) {
        has_unread = unread;
        emit unreadChanged();
    }
}

void SlackChannel::markRead(const SlackMessage &message) {
    qDebug() << "Mark read after " << message.ts;
    SlackClient* client = static_cast<SlackClient*>(parent());
    if (is_channel)
        client->markChannelRead("channel", id, message.ts);
    else if (is_group)
        client->markChannelRead("group", id, message.ts);
    else if (is_im)
        client->markChannelRead("im", id, message.ts);
    else if (is_mpim)
        client->markChannelRead("mpim", id, message.ts);

    last_read = message.when;
    setHasUnread(false);
}

/// gadgets

SlackUser::SlackUser(const QJsonValueRef &sourceRef)
{
    QJsonObject &&source = sourceRef.toObject();
    id          = source["id"].toString();
    name        = source["name"].toString();
    is_deleted  = source["is_deleted"].toBool();
}

SlackMessageAttachment::SlackMessageAttachment(const QJsonObject &source)
{
    color = QColor(QString("#%1").arg(source["color"].toString()));
    text = source["text"].toString();
    fallback = source["fallback"].toString();
    title = source["title"].toString();
}

SlackMessage::SlackMessage(const QJsonObject &source)
{
    qDebug() << "Building message from " << source;
    type = source["type"].toString();
    user = source["user"].toString();
    text = source["text"].toString();
    ts   = source["ts"].toString();
    double timespec = ts.toDouble();
    when = QDateTime::fromMSecsSinceEpoch(1000 * timespec);
    username = source["username"].toString();

    if (source.contains("attachments")) {
        for (auto attachment: source["attachments"].toArray()) {
            attachments.emplace_back(SlackMessageAttachment(attachment.toObject()));
        }
    }
}

SlackMessage::SlackMessage(const QString &user, const QString &msg)
{
    this->type = "message";
    this->user = user;
    this->text = msg;
    this->when = QDateTime::currentDateTime();
}

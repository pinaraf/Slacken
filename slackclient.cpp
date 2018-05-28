#include "slackclient.h"
#include <QDebug>
#include <QOAuthHttpServerReplyHandler>
#include <QDesktopServices>
#include <QWebSocket>

static const char *clientId = "3457590858.328048934181";
static const char *clientSecret = "673a852137cc6a09717d39c480c3d88f";
static const char *verificationToken = "wOYGN5tkgAYH69WK963i0eO4";

SlackClient::SlackClient(QObject *parent) : QObject(parent)
{
    qDebug() << "Coucou le monde";
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
        // QDesktopServices::openUrl(url);
    });

    connect(this, &SlackClient::authenticated, this, [this]() {
        //this->listConversations();

        qDebug() << "State dump ";
        // QOAuth2AuthorizationCodeFlow : none
        // QAbstractOAuth2
        qDebug() << oauth2.clientIdentifierSharedKey();
        qDebug() << oauth2.expirationAt();
        qDebug() << oauth2.refreshToken();
        qDebug() << oauth2.state();
        // QAbstractOAuth
        qDebug() << oauth2.clientIdentifier();
        qDebug() << oauth2.extraTokens();
        qDebug() << oauth2.responseType();
        qDebug() << static_cast<int>(oauth2.status());
        qDebug() << oauth2.token();
        qDebug() << "End of state dump";
    });
}

void SlackClient::login() {
    oauth2.grant();
}

void SlackClient::fire() {
    auto reply = oauth2.get(QUrl("https://slack.com/api/rtm.start"));
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        qDebug() << "Reply finished !";
        qDebug() << reply->errorString();
        QString whole_doc = reply->readAll();
        //qDebug() << whole_doc;
        qDebug() << oauth2.state();

        auto doc = QJsonDocument::fromJson(whole_doc.toUtf8());

        selfId = doc["self"].toObject()["id"].toString();
        // Instanciate each channel and user
        for (const QJsonValueRef &user: doc["users"].toArray()) {
            emit userAdded(
                m_users.insert(user.toObject()["id"].toString(), SlackUser(user)).value()
            );
        }
        for (const QJsonValueRef &channel: doc["channels"].toArray()) {
            emit channelAdded(
                m_channels.insert(channel.toObject()["id"].toString(), SlackChannel(this, channel)).value()
            );
        }
        for (const QJsonValueRef &channel: doc["groups"].toArray()) {
            emit channelAdded(
                m_channels.insert(channel.toObject()["id"].toString(), SlackChannel(this, channel)).value()
            );
        }
        for (const QJsonValueRef &channel: doc["ims"].toArray()) {
            emit channelAdded(
                m_channels.insert(channel.toObject()["id"].toString(), SlackChannel(this, channel)).value()
            );
        }

        //qDebug() << doc.toJson(QJsonDocument::Indented);
/*        QFile f("/tmp/rtm.start.json");
        f.open(QIODevice::WriteOnly);
        f.write(doc.toJson(QJsonDocument::Indented));*/

        chaussette = new QWebSocket("", QWebSocketProtocol::VersionLatest, this);
        connect(chaussette, &QWebSocket::connected, [this]() {
            qDebug() << "Chaussette en ligne !";
        });
        connect(chaussette, &QWebSocket::disconnected, []() {
            qDebug() << "Chaussette hors ligne !";
        });
        connect(chaussette, &QWebSocket::textMessageReceived, [this](const QString &msg) {
            qDebug() << "Chaussette in : " << msg;
            auto doc = QJsonDocument::fromJson(msg.toUtf8());
            qDebug() << " ==> " << doc["type"];
            if (doc["type"] == "message") {
                qDebug() << "Emitting a message";
                emit(newMessage(doc["channel"].toString(), SlackMessage(doc.object())));
            }
            /*if (doc["type"] == "hello")
                chaussette->sendTextMessage("{\"id\": 1, \"type\": \"message\", \"channel\": \"D4EPF2N22\", \"text\": \"LA CHAUSSETTE PARLE !\"}");*/
        });

        qDebug() << "Chaussette vers ... " << doc["url"].toString();
        chaussette->open(QUrl(doc["url"].toString()));
        socketMessageId = 0;
    });
}

const SlackUser & SlackClient::user(const QString &id) const
{
    return m_users.find(id).value();
}

QList<SlackChannel> SlackClient::channels() const
{
    return m_channels.values();
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
        for (const QJsonValueRef &msgJson: doc["messages"].toArray()) {
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

/// gadgets

SlackChannel::SlackChannel(SlackClient *client, const QJsonValueRef &sourceRef)
{
    QJsonObject &&source = sourceRef.toObject();
    created     = QDateTime::fromTime_t(source["created"].toInt());
    id          = source["id"].toString();
    if (source.contains("is_im") && source["is_im"].toBool()) {
        is_im = true;
        is_member = true;
        is_channel = false;
        is_group = false;
        name = client->user(source["user"].toString()).name;
    } else {
        name        = source["name"].toString();
        is_channel  = source["is_channel"].toBool();
        is_group    = source["is_group"].toBool();
        is_mpim     = source["is_mpim"].toBool();
        is_general  = source["is_general"].toBool();
        is_archived = source["is_archived"].toBool();
        is_member   = source["is_member"].toBool();
        if (is_group)
            is_member = source["is_open"].toBool();
    }
}

SlackUser::SlackUser(const QJsonValueRef &sourceRef)
{
    QJsonObject &&source = sourceRef.toObject();
    id          = source["id"].toString();
    name        = source["name"].toString();
    is_deleted  = source["is_deleted"].toBool();
}

SlackMessage::SlackMessage(const QJsonObject &source)
{
    type = source["type"].toString();
    user = source["user"].toString();
    text = source["text"].toString();
    QString ts = source["ts"].toString();
//    qDebug() << ts << ts.toInt();
    when = QDateTime::fromTime_t(ts.toDouble());
}

SlackMessage::SlackMessage(const QString &user, const QString &msg)
{
    this->type = "message";
    this->user = user;
    this->text = msg;
    this->when = QDateTime::currentDateTime();
}

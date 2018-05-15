#include "slackclient.h"
#include <QDebug>
#include <QOAuthHttpServerReplyHandler>
#include <QDesktopServices>
#include <QWebSocket>

const char *clientId = "3457590858.328048934181";
const char *clientSecret = "673a852137cc6a09717d39c480c3d88f";
const char *verificationToken = "wOYGN5tkgAYH69WK963i0eO4";

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
        qDebug() << "Status :" << (int) status;
        if (status == QAbstractOAuth::Status::Granted) {
            qDebug() << "Authenticated !";
            emit authenticated();
        }
    });
    oauth2.setModifyParametersFunction([&](QAbstractOAuth::Stage stage, QVariantMap *parameters) {
        qDebug() << "I'm lost here : stage = " << (int) stage;
        qDebug() << (*parameters);
        if (stage == QAbstractOAuth::Stage::RequestingAuthorization)
            parameters->insert("duration", "permanent");
    });
    connect(&oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, [](const QUrl &url) {
        qDebug() << "Open " << url;
        // QDesktopServices::openUrl(url);
    });

    connect(this, &SlackClient::authenticated, this, [this]() { this->listConversations(); });
}

void SlackClient::login() {
    oauth2.grant();
}

void SlackClient::fire() {
    auto reply = oauth2.get(QUrl("https://slack.com/api/rtm.connect"));
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        qDebug() << "Reply finished !";
        qDebug() << reply->errorString();
        QString whole_doc = reply->readAll();
        qDebug() << whole_doc;
        qDebug() << oauth2.state();

        auto doc = QJsonDocument::fromJson(whole_doc.toUtf8());


        QWebSocket *chaussette = new QWebSocket("", QWebSocketProtocol::VersionLatest, this);
        connect(chaussette, &QWebSocket::connected, [chaussette]() {
            qDebug() << "Chaussette en ligne !";
        });
        connect(chaussette, &QWebSocket::disconnected, []() {
            qDebug() << "Chaussette hors ligne !";
        });
        connect(chaussette, &QWebSocket::textMessageReceived, [chaussette](const QString &msg) {
            qDebug() << "Chaussette in : " << msg;
            auto doc = QJsonDocument::fromJson(msg.toUtf8());
            /*if (doc["type"] == "hello")
                chaussette->sendTextMessage("{\"id\": 1, \"type\": \"message\", \"channel\": \"D4EPF2N22\", \"text\": \"LA CHAUSSETTE PARLE !\"}");*/
        });

        qDebug() << "Chaussette vers ... " << doc["url"].toString();
        chaussette->open(QUrl(doc["url"].toString()));
    });
}

void SlackClient::listConversations(const QString &cursor)
{
    auto reply = oauth2.get(QUrl(QString("https://slack.com/api/conversations.list?cursor=%1").arg(cursor)));
    connect(reply, &QNetworkReply::finished, [this, reply] () {
        qDebug() << "Reply finished !";
        qDebug() << reply->errorString();
        QString whole_doc = reply->readAll();

        auto doc = QJsonDocument::fromJson(whole_doc.toUtf8());

        if (doc["ok"].toBool(false)) {
            QJsonArray conversations = doc["channels"].toArray();
            qDebug() << "conversations found :" << conversations.size();
            for (auto &&conversationJson: conversations) {
                SlackConversation conversationObject(conversationJson);
                m_conversations.insert(conversationObject.id, std::move(conversationObject));
            }

            if (doc["response_metadata"] != QJsonValue::Undefined) {
                QJsonObject metadata = doc["response_metadata"].toObject();
                if (metadata.contains("next_cursor") && metadata["next_cursor"] != "")
                    listConversations(metadata["next_cursor"].toString());
            }
        }
    });
}

QList<SlackConversation> SlackClient::conversations() const
{
    return m_conversations.values();
}

SlackConversation::SlackConversation(const QJsonValueRef &sourceRef)
{
    QJsonObject &&source = sourceRef.toObject();
    id          = source["id"].toString();
    name        = source["name"].toString();
    is_channel  = source["is_channel"].toBool();
    is_group    = source["is_group"].toBool();
    is_im       = source["is_im"].toBool();
    is_general  = source["is_general"].toBool();
    is_archived = source["is_archived"].toBool();
}

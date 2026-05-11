#include "../server.h"
#include "../database.h"

void ChatServer::handlePrivateMsg(QTcpSocket *sock, const QJsonObject &data)
{
    QString sender = m_clients.value(sock);
    if (sender.isEmpty()) return;

    QString target = data["target"].toString();
    QString content = data["content"].toString();

    int msgId = m_db->saveMessage(sender, target, "private", content);

    QJsonObject fwd;
    fwd["type"] = MSG_MESSAGE;
    fwd["data"] = QJsonObject{
        {"msg_id", msgId},
        {"from", sender},
        {"to", target},
        {"content", content},
        {"time", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")},
        {"msg_type", "private"}
    };
    QByteArray json = QJsonDocument(fwd).toJson(QJsonDocument::Compact);

    if (m_userMap.contains(target))
        sendPacket(m_userMap[target], json);
    sendPacket(sock, json);
}

void ChatServer::handlePublicMsg(QTcpSocket *sock, const QJsonObject &data)
{
    QString sender = m_clients.value(sock);
    if (sender.isEmpty()) return;

    QString content = data["content"].toString();

    int msgId = m_db->saveMessage(sender, "ALL", "public", content);

    QJsonObject fwd;
    fwd["type"] = MSG_MESSAGE;
    fwd["data"] = QJsonObject{
        {"msg_id", msgId},
        {"from", sender},
        {"to", "ALL"},
        {"content", content},
        {"time", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")},
        {"msg_type", "public"}
    };
    QByteArray json = QJsonDocument(fwd).toJson(QJsonDocument::Compact);

    for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
        sendPacket(it.key(), json);
}

void ChatServer::handleRecall(QTcpSocket *sock, const QJsonObject &data)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    int msgId = data["msg_id"].toInt();
    auto info = m_db->getMessageInfo(msgId);
    if (info.id < 0) {
        sendError(sock, "消息不存在");
        return;
    }

    if (info.sender != username) {
        sendError(sock, "只能撤回自己的消息");
        return;
    }

    QDateTime msgTime = QDateTime::fromString(info.timestamp, "yyyy-MM-dd hh:mm:ss");
    if (!msgTime.isValid())
        msgTime = QDateTime::fromString(info.timestamp, Qt::ISODate);
    if (msgTime.isValid() && msgTime.secsTo(QDateTime::currentDateTime()) > 120) {
        sendError(sock, "超过 2 分钟无法撤回");
        return;
    }

    m_db->markRecalled(msgId);

    QJsonObject res;
    res["type"] = MSG_RECALL_RES;
    res["data"] = QJsonObject{{"ok", true}, {"msg_id", msgId}};
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));

    QJsonObject ntf;
    ntf["type"] = MSG_RECALL_NTF;
    ntf["data"] = QJsonObject{
        {"msg_id", msgId},
        {"from", username},
        {"target", info.target},
        {"msg_type", info.msgType}
    };

    QByteArray json = QJsonDocument(ntf).toJson(QJsonDocument::Compact);

    if (info.msgType == "public") {
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
            sendPacket(it.key(), json);
    } else if (info.msgType == "private") {
        QString other = (info.target == username) ? info.sender : info.target;
        if (m_userMap.contains(other))
            sendPacket(m_userMap[other], json);
        sendPacket(sock, json);
    } else if (info.msgType == "group") {
        int groupId = info.target.toInt();
        QStringList members = m_db->getGroupMembers(groupId);
        for (const auto &m : members) {
            if (m_userMap.contains(m))
                sendPacket(m_userMap[m], json);
        }
    }
}

void ChatServer::handleOfflineQuery(QTcpSocket *sock, const QJsonObject &data)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    int lastId = data["last_id"].toInt();
    QJsonArray messages = m_db->getMessagesSince(lastId, username);

    QJsonObject res;
    res["type"] = MSG_OFFLINE_RES;
    res["data"] = QJsonObject{{"messages", messages}};
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void ChatServer::handleHistory(QTcpSocket *sock, const QJsonObject &data)
{
    QString sender = m_clients.value(sock);
    if (sender.isEmpty()) return;

    QString type = data["type"].toString();
    QString target = data["target"].toString();
    if (target.isEmpty()) target = sender;
    int limit = data["limit"].toInt(100);

    QJsonObject res;
    res["type"] = MSG_HISTORY_RES;

    if (type == "file" || type == "all") {
        res["data"] = QJsonObject{
            {"messages", QJsonArray()},
            {"files", m_db->getFiles(type, target, limit)}
        };
    } else {
        res["data"] = QJsonObject{
            {"messages", m_db->getMessages(type, target, limit)},
            {"files", QJsonArray()}
        };
    }
    QJsonObject resData = res["data"].toObject();
    resData["type"] = type;
    res["data"] = resData;
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void ChatServer::handleFileMsg(QTcpSocket *sock, const QJsonObject &data)
{
    QString sender = m_clients.value(sock);
    if (sender.isEmpty()) return;

    QString target = data["target"].toString();
    QString filename = data["filename"].toString();
    QString base64Data = data["data"].toString();
    qint64 filesize = data["filesize"].toVariant().toLongLong();

    QString content = filename + "||" + base64Data;
    int msgId = m_db->saveMessage(sender, target, "file", content);

    QJsonObject fwd;
    fwd["type"] = MSG_FILE_MSG;
    fwd["data"] = QJsonObject{
        {"msg_id", msgId},
        {"from", sender},
        {"to", target},
        {"filename", filename},
        {"filesize", filesize},
        {"data", base64Data},
        {"time", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
    };
    QByteArray json = QJsonDocument(fwd).toJson(QJsonDocument::Compact);

    if (target == "ALL") {
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
            sendPacket(it.key(), json);
    } else {
        if (m_userMap.contains(target))
            sendPacket(m_userMap[target], json);
        sendPacket(sock, json);
    }
}

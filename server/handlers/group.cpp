#include "../server.h"
#include "../database.h"

void ChatServer::handleGroupCreate(QTcpSocket *sock, const QJsonObject &data)
{
    QString creator = m_clients.value(sock);
    if (creator.isEmpty()) return;

    QString name = data["name"].toString().trimmed();
    if (name.isEmpty()) name = creator + QStringLiteral("的群聊");

    int groupId = m_db->createGroup(name, creator);
    if (groupId < 0) {
        sendError(sock, "创建群聊失败");
        return;
    }

    m_db->addGroupMember(groupId, creator, "owner");

    QJsonArray members = data["members"].toArray();
    for (const auto &m : members) {
        QString memberName = m.toString();
        if (memberName != creator && m_db->userExists(memberName))
            m_db->addGroupMember(groupId, memberName);
    }

    QJsonObject res;
    res["type"] = MSG_GROUP_CREATE_RES;
    res["data"] = QJsonObject{{"ok", true}, {"group_id", groupId}, {"name", name}};
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));

    QJsonObject ntf;
    ntf["type"] = MSG_GROUP_INVITE_NTF;
    ntf["data"] = QJsonObject{
        {"group_id", groupId},
        {"group_name", name},
        {"from", creator}
    };
    QByteArray ntfJson = QJsonDocument(ntf).toJson(QJsonDocument::Compact);
    for (const auto &m : members) {
        QString memberName = m.toString();
        if (memberName != creator && m_userMap.contains(memberName))
            sendPacket(m_userMap[memberName], ntfJson);
    }
}

void ChatServer::handleGroupInvite(QTcpSocket *sock, const QJsonObject &data)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    int groupId = data["group_id"].toInt();
    QString member = data["username"].toString();

    QJsonObject res;
    res["type"] = MSG_GROUP_INVITE_RES;

    if (!m_db->isGroupMember(groupId, username)) {
        res["data"] = QJsonObject{{"ok", false}, {"message", "你不是群成员"}};
    } else if (!m_db->userExists(member)) {
        res["data"] = QJsonObject{{"ok", false}, {"message", "用户不存在"}};
    } else if (m_db->isGroupMember(groupId, member)) {
        res["data"] = QJsonObject{{"ok", false}, {"message", "该用户已是群成员"}};
    } else if (m_db->addGroupMember(groupId, member)) {
        res["data"] = QJsonObject{{"ok", true}, {"group_id", groupId}};
        sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));

        QJsonObject ntf;
        ntf["type"] = MSG_GROUP_INVITE_NTF;
        ntf["data"] = QJsonObject{
            {"group_id", groupId},
            {"group_name", m_db->getGroupName(groupId)},
            {"from", username}
        };
        if (m_userMap.contains(member))
            sendPacket(m_userMap[member], QJsonDocument(ntf).toJson(QJsonDocument::Compact));
        return;
    } else {
        res["data"] = QJsonObject{{"ok", false}, {"message", "邀请失败"}};
    }
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void ChatServer::handleGroupMsg(QTcpSocket *sock, const QJsonObject &data)
{
    QString sender = m_clients.value(sock);
    if (sender.isEmpty()) return;

    int groupId = data["group_id"].toInt();
    QString content = data["content"].toString();

    if (!m_db->isGroupMember(groupId, sender)) {
        sendError(sock, "你不是群成员");
        return;
    }

    int msgId = m_db->saveMessage(sender, QString::number(groupId), "group", content);

    QJsonObject fwd;
    fwd["type"] = MSG_GROUP_MSG;
    fwd["data"] = QJsonObject{
        {"msg_id", msgId},
        {"group_id", groupId},
        {"from", sender},
        {"content", content},
        {"time", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
    };

    QStringList members = m_db->getGroupMembers(groupId);
    QByteArray json = QJsonDocument(fwd).toJson(QJsonDocument::Compact);
    for (const auto &m : members) {
        if (m_userMap.contains(m))
            sendPacket(m_userMap[m], json);
    }
}

void ChatServer::handleGroupList(QTcpSocket *sock)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    QJsonObject res;
    res["type"] = MSG_GROUP_LIST_RES;
    res["data"] = QJsonObject{{"groups", m_db->getUserGroups(username)}};
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void ChatServer::handleGroupLeave(QTcpSocket *sock, const QJsonObject &data)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    int groupId = data["group_id"].toInt();
    m_db->removeGroupMember(groupId, username);

    QJsonObject res;
    res["type"] = MSG_GROUP_LEAVE_RES;
    res["data"] = QJsonObject{{"ok", true}, {"group_id", groupId}};
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void ChatServer::handleGroupMembers(QTcpSocket *sock, const QJsonObject &data)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    int groupId = data["group_id"].toInt();

    if (!m_db->isGroupMember(groupId, username)) {
        sendError(sock, "你不是群成员");
        return;
    }

    QStringList members = m_db->getGroupMembers(groupId);
    QJsonArray arr;
    for (const auto &m : members) {
        QJsonObject obj;
        obj["username"] = m;
        obj["online"] = m_onlineUsers.contains(m);
        arr.append(obj);
    }

    QJsonObject res;
    res["type"] = MSG_GROUP_MEMBERS_RES;
    res["data"] = QJsonObject{{"group_id", groupId}, {"members", arr}};
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void ChatServer::handleGroupFileMsg(QTcpSocket *sock, const QJsonObject &data)
{
    QString sender = m_clients.value(sock);
    if (sender.isEmpty()) return;

    int groupId = data["group_id"].toInt();
    QString filename = data["filename"].toString();
    QString base64Data = data["data"].toString();
    qint64 filesize = data["filesize"].toVariant().toLongLong();

    if (!m_db->isGroupMember(groupId, sender)) {
        sendError(sock, "你不是群成员");
        return;
    }

    QString content = filename + "||" + base64Data;
    int msgId = m_db->saveMessage(sender, QString::number(groupId), "file", content);
    m_db->saveFileRecord(sender, QString::number(groupId), "group", filename, "inline", filesize);

    QJsonObject fwd;
    fwd["type"] = MSG_GROUP_FILE_MSG;
    fwd["data"] = QJsonObject{
        {"msg_id", msgId},
        {"group_id", groupId},
        {"from", sender},
        {"filename", filename},
        {"filesize", filesize},
        {"data", base64Data},
        {"time", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")}
    };

    QStringList members = m_db->getGroupMembers(groupId);
    QByteArray json = QJsonDocument(fwd).toJson(QJsonDocument::Compact);
    for (const auto &m : members) {
        if (m_userMap.contains(m))
            sendPacket(m_userMap[m], json);
    }
}

#include "../server.h"
#include "../database.h"

void ChatServer::handleFriendRequest(QTcpSocket *sock, const QJsonObject &data)
{
    QString from = m_clients.value(sock);
    if (from.isEmpty()) return;

    QString target = data["target"].toString();

    QJsonObject res;
    res["type"] = MSG_FRIEND_REQUEST_RES;

    if (target == from) {
        res["data"] = QJsonObject{{"ok", false}, {"message", "不能添加自己为好友"}};
    } else if (!m_db->userExists(target)) {
        res["data"] = QJsonObject{{"ok", false}, {"message", "用户不存在"}};
    } else if (m_db->areFriends(from, target)) {
        res["data"] = QJsonObject{{"ok", false}, {"message", "已经是好友"}};
    } else if (m_db->hasPendingRequest(from, target)) {
        res["data"] = QJsonObject{{"ok", false}, {"message", "已发送过好友请求，等待对方接受"}};
    } else if (m_db->hasPendingRequest(target, from)) {
        m_db->acceptFriendRequest(from, target);
        res["data"] = QJsonObject{{"ok", true}, {"message", "对方已向您发送过请求，已自动成为好友"}};
        sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));

        QJsonObject ntfToFrom;
        ntfToFrom["type"] = MSG_FRIEND_ACCEPT_NTF;
        ntfToFrom["data"] = QJsonObject{{"from", target}};
        if (m_userMap.contains(from))
            sendPacket(m_userMap[from], QJsonDocument(ntfToFrom).toJson(QJsonDocument::Compact));

        QJsonObject ntfToTarget;
        ntfToTarget["type"] = MSG_FRIEND_ACCEPT_NTF;
        ntfToTarget["data"] = QJsonObject{{"from", from}};
        if (m_userMap.contains(target))
            sendPacket(m_userMap[target], QJsonDocument(ntfToTarget).toJson(QJsonDocument::Compact));
        return;
    } else if (m_db->sendFriendRequest(from, target)) {
        res["data"] = QJsonObject{{"ok", true}, {"message", "好友请求已发送"}};
        sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));

        if (m_userMap.contains(target)) {
            QJsonObject ntf;
            ntf["type"] = MSG_FRIEND_INCOMING;
            ntf["data"] = QJsonObject{{"from", from}};
            sendPacket(m_userMap[target], QJsonDocument(ntf).toJson(QJsonDocument::Compact));
        }
        return;
    } else {
        res["data"] = QJsonObject{{"ok", false}, {"message", "请求发送失败"}};
    }
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void ChatServer::handleFriendAccept(QTcpSocket *sock, const QJsonObject &data)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    QString requester = data["from"].toString();

    QJsonObject res;
    res["type"] = MSG_FRIEND_ACCEPT_RES;

    if (m_db->acceptFriendRequest(username, requester)) {
        res["data"] = QJsonObject{{"ok", true}};
        sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));

        if (m_userMap.contains(requester)) {
            QJsonObject ntf;
            ntf["type"] = MSG_FRIEND_ACCEPT_NTF;
            ntf["data"] = QJsonObject{{"from", username}};
            sendPacket(m_userMap[requester], QJsonDocument(ntf).toJson(QJsonDocument::Compact));
        }
    } else {
        res["data"] = QJsonObject{{"ok", false}, {"message", "接受失败"}};
        sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
    }
}

void ChatServer::handleFriendReject(QTcpSocket *sock, const QJsonObject &data)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    QString requester = data["from"].toString();

    QJsonObject res;
    res["type"] = MSG_FRIEND_REJECT_RES;
    if (m_db->rejectFriendRequest(username, requester)) {
        res["data"] = QJsonObject{{"ok", true}};
    } else {
        res["data"] = QJsonObject{{"ok", false}, {"message", "操作失败"}};
    }
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void ChatServer::handleFriendList(QTcpSocket *sock)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    QStringList friends = m_db->getFriendList(username);
    QJsonArray arr;
    for (const auto &f : friends) {
        QJsonObject obj;
        obj["username"] = f;
        obj["online"] = m_onlineUsers.contains(f);
        arr.append(obj);
    }

    QJsonObject res;
    res["type"] = MSG_FRIEND_LIST_RES;
    res["data"] = QJsonObject{{"friends", arr}};
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void ChatServer::handleFriendRemove(QTcpSocket *sock, const QJsonObject &data)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    QString target = data["target"].toString();
    QJsonObject res;
    res["type"] = MSG_FRIEND_REMOVE_RES;
    res["data"] = QJsonObject{{"ok", m_db->removeFriendship(username, target)}};
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void ChatServer::handleFriendPendingList(QTcpSocket *sock)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    QStringList incoming = m_db->getPendingRequests(username);
    QStringList outgoing = m_db->getOutgoingRequests(username);

    QJsonArray incomingArr, outgoingArr;
    for (const auto &u : incoming) {
        QJsonObject obj;
        obj["username"] = u;
        obj["online"] = m_onlineUsers.contains(u);
        incomingArr.append(obj);
    }
    for (const auto &u : outgoing) {
        QJsonObject obj;
        obj["username"] = u;
        obj["online"] = m_onlineUsers.contains(u);
        outgoingArr.append(obj);
    }

    QJsonObject res;
    res["type"] = MSG_FRIEND_PENDING_LIST_RES;
    res["data"] = QJsonObject{
        {"incoming", incomingArr},
        {"outgoing", outgoingArr}
    };
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

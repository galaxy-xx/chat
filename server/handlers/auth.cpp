#include "../server.h"
#include "../database.h"

void ChatServer::handleRegister(QTcpSocket *sock, const QJsonObject &data)
{
    QString user = data["username"].toString().trimmed();
    QString pass = data["password"].toString();

    QJsonObject res;
    res["type"] = MSG_REGISTER_RES;

    if (user.isEmpty() || pass.isEmpty() || user.size() > MAX_NAME_LEN) {
        res["data"] = QJsonObject{{"ok", false}, {"message", "无效的用户名或密码"}};
    } else if (m_db->userExists(user)) {
        res["data"] = QJsonObject{{"ok", false}, {"message", "用户名已存在"}};
    } else if (m_db->registerUser(user, pass)) {
        res["data"] = QJsonObject{{"ok", true}, {"message", "注册成功"}};
        qInfo() << "新用户注册：" << user;
    } else {
        res["data"] = QJsonObject{{"ok", false}, {"message", "注册失败"}};
    }
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void ChatServer::handleLogin(QTcpSocket *sock, const QJsonObject &data)
{
    QString user = data["username"].toString().trimmed();
    QString pass = data["password"].toString();

    QJsonObject res;
    res["type"] = MSG_LOGIN_RES;

    if (m_onlineUsers.contains(user)) {
        res["data"] = QJsonObject{{"ok", false}, {"message", "用户已在线"}};
    } else if (m_db->loginUser(user, pass)) {
        QString oldUser = m_clients.value(sock);
        if (!oldUser.isEmpty()) {
            m_onlineUsers.remove(oldUser);
            m_userMap.remove(oldUser);
        }

        m_clients[sock] = user;
        m_userMap[user] = sock;
        m_onlineUsers.insert(user);

        res["data"] = QJsonObject{{"ok", true}, {"message", "登录成功"}};
        sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));

        broadcastUserList();
        notifyUserStatus(user, true);

        // 登录后下发待处理的好友请求
        QStringList pending = m_db->getPendingRequests(user);
        for (const auto &requester : pending) {
            QJsonObject ntf;
            ntf["type"] = MSG_FRIEND_INCOMING;
            ntf["data"] = QJsonObject{{"from", requester}};
            sendPacket(sock, QJsonDocument(ntf).toJson(QJsonDocument::Compact));
            qInfo() << "  待处理请求:" << requester << "→" << user;
        }

        qInfo() << "用户登录：" << user;
    } else {
        res["data"] = QJsonObject{{"ok", false}, {"message", "用户名或密码错误"}};
        sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
    }
}

void ChatServer::handleLogout(QTcpSocket *sock)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    m_onlineUsers.remove(username);
    m_userMap.remove(username);
    m_clients.remove(sock);

    QJsonObject res;
    res["type"] = MSG_LOGOUT_RES;
    res["data"] = QJsonObject{{"ok", true}};
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));

    broadcastUserList();
    notifyUserStatus(username, false);
    qInfo() << "用户登出：" << username;
}

void ChatServer::handleDeleteAccount(QTcpSocket *sock)
{
    QString username = m_clients.value(sock);
    if (username.isEmpty()) return;

    m_onlineUsers.remove(username);
    m_userMap.remove(username);
    m_clients.remove(sock);

    bool ok = m_db->deleteUser(username);

    QJsonObject res;
    res["type"] = MSG_DELETE_ACCOUNT_RES;
    res["data"] = QJsonObject{{"ok", ok}};
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));

    broadcastUserList();
    notifyUserStatus(username, false);
    qInfo() << "用户注销：" << username;
}

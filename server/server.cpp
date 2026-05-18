#include "server.h"
#include "database.h"

ChatServer::ChatServer(Database *db, const QString &storageDir, QObject *parent)
    : QObject(parent), m_db(db), m_storageDir(storageDir)
{
    QDir().mkpath(storageDir);
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &ChatServer::onNewConnection);
}

bool ChatServer::start(quint16 port)
{
    if (!m_server->listen(QHostAddress::Any, port)) {
        qCritical() << "Server cannot listen on port" << port;
        return false;
    }
    qInfo() << "Chat server started on port" << port;
    return true;
}

void ChatServer::broadcastUserList()
{
    QJsonObject msg;
    msg["type"] = MSG_USER_LIST_RES;
    QJsonArray users;
    for (const auto &u : m_onlineUsers)
        users.append(u);
    msg["data"] = QJsonObject{{"users", users}};
    QByteArray json = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
        sendPacket(it.key(), json);
}

void ChatServer::notifyUserStatus(const QString &username, bool online)
{
    QJsonObject msg;
    msg["type"] = MSG_USER_STATUS;
    msg["data"] = QJsonObject{{"username", username}, {"online", online}};
    QByteArray json = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
        sendPacket(it.key(), json);
}

void ChatServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *sock = m_server->nextPendingConnection();
        connect(sock, &QTcpSocket::readyRead, this, &ChatServer::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &ChatServer::onDisconnected);
        qInfo() << "新连接来自" << sock->peerAddress().toString();
    }
}

void ChatServer::onReadyRead()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    m_recvBuf[sock].append(sock->readAll());

    QByteArray &buf = m_recvBuf[sock];
    while (buf.size() >= 4) {
        uint32_t len = unpack32(reinterpret_cast<const uint8_t*>(buf.data()));
        if (buf.size() < 4 + len) break;
        QByteArray payload = buf.mid(4, len);
        buf.remove(0, 4 + len);
        processMessage(sock, payload);
    }
}

void ChatServer::onDisconnected()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    QString username = m_clients.value(sock);
    if (!username.isEmpty()) {
        m_onlineUsers.remove(username);
        m_userMap.remove(username);
        qInfo() << "用户登出：" << username;
        broadcastUserList();
        notifyUserStatus(username, false);
    }
    m_clients.remove(sock);
    m_recvBuf.remove(sock);
    sock->deleteLater();
}

void ChatServer::processMessage(QTcpSocket *sock, const QByteArray &payload)
{
    QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) return;
    QJsonObject msg = doc.object();
    QString type = msg["type"].toString();
    QJsonObject data = msg["data"].toObject();

    QString username = m_clients.value(sock);

    if (type == MSG_REGISTER)           handleRegister(sock, data);
    else if (type == MSG_LOGIN)         handleLogin(sock, data);
    else if (type == MSG_LOGOUT)        handleLogout(sock);
    else if (type == MSG_DELETE_ACCOUNT) handleDeleteAccount(sock);
    else if (type == MSG_PRIVATE_MSG)   handlePrivateMsg(sock, data);
    else if (type == MSG_PUBLIC_MSG)    handlePublicMsg(sock, data);
    else if (type == MSG_FILE_META)     handleFileMeta(sock, data);
    else if (type == MSG_FILE_DATA)     handleFileData(sock, data);
    else if (type == MSG_FILE_END)      handleFileEnd(sock, data);
    else if (type == MSG_FILE_ACCEPT)   handleFileAccept(sock, data);
    else if (type == MSG_HISTORY)       handleHistory(sock, data);
    else if (type == MSG_USER_LIST)     handleUserList(sock);
    else if (type == MSG_RECALL)        handleRecall(sock, data);
    else if (type == MSG_OFFLINE_QUERY) handleOfflineQuery(sock, data);
    else if (type == MSG_FRIEND_REQUEST)    handleFriendRequest(sock, data);
    else if (type == MSG_FRIEND_ACCEPT)     handleFriendAccept(sock, data);
    else if (type == MSG_FRIEND_REJECT)     handleFriendReject(sock, data);
    else if (type == MSG_FRIEND_LIST)       handleFriendList(sock);
    else if (type == MSG_FRIEND_REMOVE)     handleFriendRemove(sock, data);
    else if (type == MSG_FRIEND_PENDING_LIST) handleFriendPendingList(sock);
    else if (type == MSG_GROUP_CREATE)  handleGroupCreate(sock, data);
    else if (type == MSG_GROUP_INVITE)  handleGroupInvite(sock, data);
    else if (type == MSG_GROUP_MSG)     handleGroupMsg(sock, data);
    else if (type == MSG_GROUP_LIST)    handleGroupList(sock);
    else if (type == MSG_GROUP_LEAVE)   handleGroupLeave(sock, data);
    else if (type == MSG_GROUP_MEMBERS) handleGroupMembers(sock, data);
    else if (type == MSG_FILE_MSG)      handleFileMsg(sock, data);
    else if (type == MSG_GROUP_FILE_MSG) handleGroupFileMsg(sock, data);
    else sendError(sock, "未知消息类型");
}

void ChatServer::sendError(QTcpSocket *sock, const QString &msg)
{
    QJsonObject res;
    res["type"] = MSG_ERROR;
    res["data"] = QJsonObject{{"message", msg}};
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void ChatServer::sendPacket(QTcpSocket *sock, const QByteArray &json)
{
    uint8_t header[4];
    pack32(json.size(), header);
    sock->write(reinterpret_cast<const char*>(header), 4);
    sock->write(json);
    sock->flush();
}

void ChatServer::sendJsonToUser(const QString &username, const QJsonObject &msg)
{
    if (m_userMap.contains(username)) {
        QByteArray json = QJsonDocument(msg).toJson(QJsonDocument::Compact);
        sendPacket(m_userMap[username], json);
    }
}

void ChatServer::handleUserList(QTcpSocket *sock)
{
    QJsonObject res;
    res["type"] = MSG_USER_LIST_RES;
    QJsonArray users;
    for (const auto &u : m_onlineUsers)
        users.append(u);
    res["data"] = QJsonObject{{"users", users}};
    sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
}

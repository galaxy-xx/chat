#include "clientnetwork.h"

ClientNetwork::ClientNetwork(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &ClientNetwork::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientNetwork::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientNetwork::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &ClientNetwork::onError);
}

void ClientNetwork::connectToServer(const QString &host, quint16 port)
{
    m_recvBuf.clear();
    m_socket->connectToHost(host, port);
}

void ClientNetwork::disconnect()
{
    m_socket->disconnectFromHost();
}

bool ClientNetwork::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void ClientNetwork::sendMessage(const QJsonObject &msg)
{
    QByteArray json = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    sendRaw(json);
}

void ClientNetwork::sendRaw(const QByteArray &data)
{
    if (!isConnected()) return;
    uint8_t header[4];
    pack32(data.size(), header);
    m_socket->write(reinterpret_cast<const char*>(header), 4);
    m_socket->write(data);
    m_socket->flush();
}

void ClientNetwork::sendRegister(const QString &user, const QString &pass)
{
    QJsonObject msg;
    msg["type"] = MSG_REGISTER;
    msg["data"] = QJsonObject{{"username", user}, {"password", pass}};
    sendMessage(msg);
}

void ClientNetwork::sendLogin(const QString &user, const QString &pass)
{
    QJsonObject msg;
    msg["type"] = MSG_LOGIN;
    msg["data"] = QJsonObject{{"username", user}, {"password", pass}};
    sendMessage(msg);
}

void ClientNetwork::sendLogout()
{
    QJsonObject msg;
    msg["type"] = MSG_LOGOUT;
    sendMessage(msg);
}

void ClientNetwork::sendPrivateMsg(const QString &target, const QString &content)
{
    QJsonObject msg;
    msg["type"] = MSG_PRIVATE_MSG;
    msg["data"] = QJsonObject{{"target", target}, {"content", content}};
    sendMessage(msg);
}

void ClientNetwork::sendPublicMsg(const QString &content)
{
    QJsonObject msg;
    msg["type"] = MSG_PUBLIC_MSG;
    msg["data"] = QJsonObject{{"content", content}};
    sendMessage(msg);
}

void ClientNetwork::sendFileMsg(const QString &target, const QString &filename,
                                 qint64 filesize, const QString &base64Data)
{
    QJsonObject msg;
    msg["type"] = MSG_FILE_MSG;
    msg["data"] = QJsonObject{
        {"target", target},
        {"filename", filename},
        {"filesize", filesize},
        {"data", base64Data}
    };
    sendMessage(msg);
}

void ClientNetwork::sendGroupFileMsg(int groupId, const QString &filename,
                                      qint64 filesize, const QString &base64Data)
{
    QJsonObject msg;
    msg["type"] = MSG_GROUP_FILE_MSG;
    msg["data"] = QJsonObject{
        {"group_id", groupId},
        {"filename", filename},
        {"filesize", filesize},
        {"data", base64Data}
    };
    sendMessage(msg);
}

void ClientNetwork::sendHistoryQuery(const QString &type, const QString &target, int limit)
{
    QJsonObject msg;
    msg["type"] = MSG_HISTORY;
    msg["data"] = QJsonObject{{"type", type}, {"target", target}, {"limit", limit}};
    sendMessage(msg);
}

void ClientNetwork::sendUserList()
{
    QJsonObject msg;
    msg["type"] = MSG_USER_LIST;
    sendMessage(msg);
}

// ---- 消息撤回 ----
void ClientNetwork::sendRecall(int msgId)
{
    QJsonObject msg;
    msg["type"] = MSG_RECALL;
    msg["data"] = QJsonObject{{"msg_id", msgId}};
    sendMessage(msg);
}

// ---- 好友系统 ----
void ClientNetwork::sendFriendRequest(const QString &target)
{
    QJsonObject msg;
    msg["type"] = MSG_FRIEND_REQUEST;
    msg["data"] = QJsonObject{{"target", target}};
    sendMessage(msg);
}

void ClientNetwork::sendFriendAccept(const QString &from)
{
    QJsonObject msg;
    msg["type"] = MSG_FRIEND_ACCEPT;
    msg["data"] = QJsonObject{{"from", from}};
    sendMessage(msg);
}

void ClientNetwork::sendFriendReject(const QString &from)
{
    QJsonObject msg;
    msg["type"] = MSG_FRIEND_REJECT;
    msg["data"] = QJsonObject{{"from", from}};
    sendMessage(msg);
}

void ClientNetwork::sendFriendList()
{
    QJsonObject msg;
    msg["type"] = MSG_FRIEND_LIST;
    sendMessage(msg);
}

void ClientNetwork::sendFriendRemove(const QString &target)
{
    QJsonObject msg;
    msg["type"] = MSG_FRIEND_REMOVE;
    msg["data"] = QJsonObject{{"target", target}};
    sendMessage(msg);
}

// ---- 群聊 ----
void ClientNetwork::sendGroupCreate(const QString &name, const QStringList &members)
{
    QJsonObject msg;
    msg["type"] = MSG_GROUP_CREATE;
    QJsonArray arr;
    for (const auto &m : members)
        arr.append(m);
    msg["data"] = QJsonObject{{"name", name}, {"members", arr}};
    sendMessage(msg);
}

void ClientNetwork::sendGroupMsg(int groupId, const QString &content)
{
    QJsonObject msg;
    msg["type"] = MSG_GROUP_MSG;
    msg["data"] = QJsonObject{{"group_id", groupId}, {"content", content}};
    sendMessage(msg);
}

void ClientNetwork::sendGroupList()
{
    QJsonObject msg;
    msg["type"] = MSG_GROUP_LIST;
    sendMessage(msg);
}

void ClientNetwork::sendGroupLeave(int groupId)
{
    QJsonObject msg;
    msg["type"] = MSG_GROUP_LEAVE;
    msg["data"] = QJsonObject{{"group_id", groupId}};
    sendMessage(msg);
}

void ClientNetwork::sendGroupInvite(int groupId, const QString &username)
{
    QJsonObject msg;
    msg["type"] = MSG_GROUP_INVITE;
    msg["data"] = QJsonObject{{"group_id", groupId}, {"username", username}};
    sendMessage(msg);
}

void ClientNetwork::sendGroupMembers(int groupId)
{
    QJsonObject msg;
    msg["type"] = MSG_GROUP_MEMBERS;
    msg["data"] = QJsonObject{{"group_id", groupId}};
    sendMessage(msg);
}

// ---- 离线消息 ----
void ClientNetwork::sendOfflineQuery(int lastMsgId)
{
    QJsonObject msg;
    msg["type"] = MSG_OFFLINE_QUERY;
    msg["data"] = QJsonObject{{"last_id", lastMsgId}};
    sendMessage(msg);
}

// ---- 待处理好友请求 ----
void ClientNetwork::sendFriendPendingList()
{
    QJsonObject msg;
    msg["type"] = MSG_FRIEND_PENDING_LIST;
    sendMessage(msg);
}

// ---------- 槽函数 ----------
void ClientNetwork::onConnected()
{
    emit connected();
}

void ClientNetwork::onDisconnected()
{
    emit disconnected();
}

void ClientNetwork::onReadyRead()
{
    m_recvBuf.append(m_socket->readAll());

    while (m_recvBuf.size() >= 4) {
        uint32_t len = unpack32(reinterpret_cast<const uint8_t*>(m_recvBuf.data()));
        if ((uint32_t)m_recvBuf.size() < 4 + len) break;

        QByteArray payload = m_recvBuf.mid(4, len);
        m_recvBuf.remove(0, 4 + len);

        QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (doc.isObject()) {
            emit messageReceived(doc.object());
        }
    }
}

void ClientNetwork::onError(QAbstractSocket::SocketError)
{
    emit connectionError(m_socket->errorString());
}

#ifndef CLIENTNETWORK_H
#define CLIENTNETWORK_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QByteArray>
#include <QHostAddress>
#include <QTimer>

#include "../protocol.h"

class ClientNetwork : public QObject
{
    Q_OBJECT
public:
    explicit ClientNetwork(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);
    void disconnect();
    bool isConnected() const;

    // 通用发送
    void sendMessage(const QJsonObject &msg);
    void sendRaw(const QByteArray &data);

    // 认证
    void sendRegister(const QString &user, const QString &pass);
    void sendLogin(const QString &user, const QString &pass);
    void sendLogout();

    // 聊天
    void sendPrivateMsg(const QString &target, const QString &content);
    void sendPublicMsg(const QString &content);

    // 文件（直接发送到聊天框）
    void sendFileMsg(const QString &target, const QString &filename,
                     qint64 filesize, const QString &base64Data);
    void sendHistoryQuery(const QString &type, const QString &target, int limit = 100);
    void sendUserList();

    // 消息撤回
    void sendRecall(int msgId);

    // 好友系统
    void sendFriendRequest(const QString &target);
    void sendFriendAccept(const QString &from);
    void sendFriendReject(const QString &from);
    void sendFriendList();
    void sendFriendRemove(const QString &target);

    // 群聊
    void sendGroupCreate(const QString &name, const QStringList &members);
    void sendGroupInvite(int groupId, const QString &username);
    void sendGroupMsg(int groupId, const QString &content);
    void sendGroupList();
    void sendGroupLeave(int groupId);
    void sendGroupMembers(int groupId);

    // 离线消息
    void sendOfflineQuery(int lastMsgId);

    // 待处理好友请求
    void sendFriendPendingList();

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &error);
    void messageReceived(const QJsonObject &msg);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);

private:
    QTcpSocket *m_socket;
    QByteArray m_recvBuf;
};

#endif // CLIENTNETWORK_H

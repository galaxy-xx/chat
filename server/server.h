#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QSet>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QDebug>

#include "../protocol.h"

class Database;

class ChatServer : public QObject
{
    Q_OBJECT
public:
    ChatServer(Database *db, const QString &storageDir, QObject *parent = nullptr);
    bool start(quint16 port);
    void broadcastUserList();
    void notifyUserStatus(const QString &username, bool online);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void processMessage(QTcpSocket *sock, const QByteArray &payload);

    // 工具方法
    void sendError(QTcpSocket *sock, const QString &msg);
    void sendPacket(QTcpSocket *sock, const QByteArray &json);
    void sendJsonToUser(const QString &username, const QJsonObject &msg);

    // 认证
    void handleRegister(QTcpSocket *sock, const QJsonObject &data);
    void handleLogin(QTcpSocket *sock, const QJsonObject &data);
    void handleLogout(QTcpSocket *sock);

    // 聊天
    void handlePrivateMsg(QTcpSocket *sock, const QJsonObject &data);
    void handlePublicMsg(QTcpSocket *sock, const QJsonObject &data);
    void handleRecall(QTcpSocket *sock, const QJsonObject &data);
    void handleOfflineQuery(QTcpSocket *sock, const QJsonObject &data);
    void handleHistory(QTcpSocket *sock, const QJsonObject &data);

    // 好友
    void handleFriendRequest(QTcpSocket *sock, const QJsonObject &data);
    void handleFriendAccept(QTcpSocket *sock, const QJsonObject &data);
    void handleFriendReject(QTcpSocket *sock, const QJsonObject &data);
    void handleFriendList(QTcpSocket *sock);
    void handleFriendRemove(QTcpSocket *sock, const QJsonObject &data);
    void handleFriendPendingList(QTcpSocket *sock);

    // 群聊
    void handleGroupCreate(QTcpSocket *sock, const QJsonObject &data);
    void handleGroupInvite(QTcpSocket *sock, const QJsonObject &data);
    void handleGroupMsg(QTcpSocket *sock, const QJsonObject &data);
    void handleGroupList(QTcpSocket *sock);
    void handleGroupLeave(QTcpSocket *sock, const QJsonObject &data);
    void handleGroupMembers(QTcpSocket *sock, const QJsonObject &data);

    // 文件
    void handleFileMeta(QTcpSocket *sock, const QJsonObject &data);
    void handleFileData(QTcpSocket *sock, const QJsonObject &data);
    void handleFileEnd(QTcpSocket *sock, const QJsonObject &data);
    void handleFileAccept(QTcpSocket *sock, const QJsonObject &data);
    void handleFileMsg(QTcpSocket *sock, const QJsonObject &data);
    void handleGroupFileMsg(QTcpSocket *sock, const QJsonObject &data);

    void handleUserList(QTcpSocket *sock);

    // 成员变量
    QTcpServer *m_server = nullptr;
    Database *m_db = nullptr;
    QMap<QTcpSocket*, QString> m_clients;
    QMap<QString, QTcpSocket*> m_userMap;
    QSet<QString> m_onlineUsers;
    QMap<QTcpSocket*, QByteArray> m_recvBuf;

    struct FileTransfer {
        QString sender;
        QString target;
        QString filename;
        QString filepath;
        int totalChunks = 0;
        int chunksRecv = 0;
        QFile *file = nullptr;
    };
    QMap<int, FileTransfer> m_fileTransfers;
    int m_nextFileId = 1;
    QString m_storageDir;
};

#endif // CHATSERVER_H

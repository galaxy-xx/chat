#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QString>

class Database
{
    QSqlDatabase m_db;
public:
    bool open(const QString &path);
    void initTables();

    // 用户
    bool registerUser(const QString &user, const QString &pass);
    bool loginUser(const QString &user, const QString &pass);
    bool userExists(const QString &user);

    // 消息
    int saveMessage(const QString &sender, const QString &target,
                     const QString &type, const QString &content);
    QJsonArray getMessages(const QString &type, const QString &target,
                           int limit = 100);

    struct MessageInfo {
        int id;
        QString sender;
        QString target;
        QString msgType;
        QString timestamp;
    };
    MessageInfo getMessageInfo(int msgId);
    bool markRecalled(int msgId);
    QJsonArray getMessagesSince(int lastId, const QString &username);

    // 好友
    bool sendFriendRequest(const QString &from, const QString &to);
    bool acceptFriendRequest(const QString &responder, const QString &requester);
    bool rejectFriendRequest(const QString &responder, const QString &requester);
    bool removeFriendship(const QString &user1, const QString &user2);
    bool areFriends(const QString &a, const QString &b);
    QStringList getFriendList(const QString &username);
    bool hasPendingRequest(const QString &from, const QString &to);
    QStringList getPendingRequests(const QString &username);
    QStringList getOutgoingRequests(const QString &username);

    // 群组
    int createGroup(const QString &name, const QString &creator);
    bool addGroupMember(int groupId, const QString &username, const QString &role = "member");
    bool removeGroupMember(int groupId, const QString &username);
    bool isGroupMember(int groupId, const QString &username);
    QStringList getGroupMembers(int groupId);
    QJsonArray getUserGroups(const QString &username);
    QString getGroupName(int groupId);

    // 文件
    int saveFileRecord(const QString &sender, const QString &target,
                       const QString &fileType, const QString &filename,
                       const QString &filepath, qint64 size);
    QJsonArray getFiles(const QString &type, const QString &target,
                        int limit = 100);
};

#endif // DATABASE_H

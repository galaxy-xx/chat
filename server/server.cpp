#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QDebug>

// POSIX 头文件（守护进程相关）
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../protocol.h"

// ============================================================
// 守护进程初始化（Linux 特有）
// ============================================================
static void daemonize()
{
    pid_t pid = fork();
    if (pid < 0) {
        qCritical("fork failed");
        exit(1);
    }
    if (pid > 0) exit(0);

    setsid();
    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
}

// ============================================================
// 数据库管理
// ============================================================
class Database
{
    QSqlDatabase m_db;
public:
    bool open(const QString &path)
    {
        m_db = QSqlDatabase::addDatabase("QSQLITE");
        m_db.setDatabaseName(path);
        if (!m_db.open()) {
            qCritical() << "Failed to open database:" << m_db.lastError().text();
            return false;
        }
        initTables();
        return true;
    }

    void initTables()
    {
        QSqlQuery q(m_db);

        // 用户表
        q.exec("CREATE TABLE IF NOT EXISTS users ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "username TEXT UNIQUE NOT NULL,"
               "password TEXT NOT NULL,"
               "created_at TEXT DEFAULT (datetime('now','localtime'))"
               ")");

        // 消息表
        q.exec("CREATE TABLE IF NOT EXISTS messages ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "sender TEXT NOT NULL,"
               "target TEXT NOT NULL,"
               "msg_type TEXT NOT NULL,"
               "content TEXT NOT NULL,"
               "recalled INTEGER DEFAULT 0,"
               "timestamp TEXT DEFAULT (datetime('now','localtime'))"
               ")");

        // 文件记录表
        q.exec("CREATE TABLE IF NOT EXISTS files ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "sender TEXT NOT NULL,"
               "target TEXT NOT NULL,"
               "file_type TEXT NOT NULL,"
               "filename TEXT NOT NULL,"
               "filepath TEXT NOT NULL,"
               "filesize INTEGER DEFAULT 0,"
               "status TEXT DEFAULT 'complete',"
               "timestamp TEXT DEFAULT (datetime('now','localtime'))"
               ")");

        // 好友关系表
        q.exec("CREATE TABLE IF NOT EXISTS friendships ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "requester TEXT NOT NULL,"
               "responder TEXT NOT NULL,"
               "status TEXT DEFAULT 'pending',"
               "created_at TEXT DEFAULT (datetime('now','localtime')),"
               "UNIQUE(requester, responder)"
               ")");

        // 群组表
        q.exec("CREATE TABLE IF NOT EXISTS groups_tbl ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "name TEXT NOT NULL,"
               "creator TEXT NOT NULL,"
               "created_at TEXT DEFAULT (datetime('now','localtime'))"
               ")");

        // 群成员表
        q.exec("CREATE TABLE IF NOT EXISTS group_members ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "group_id INTEGER NOT NULL,"
               "username TEXT NOT NULL,"
               "role TEXT DEFAULT 'member',"
               "joined_at TEXT DEFAULT (datetime('now','localtime')),"
               "UNIQUE(group_id, username)"
               ")");

        // 迁移：旧数据库可能缺少 recalled 列
        q.exec("ALTER TABLE messages ADD COLUMN recalled INTEGER DEFAULT 0");

        qInfo("Database tables initialized");
    }

    // ---- 用户 ----
    bool registerUser(const QString &user, const QString &pass)
    {
        QSqlQuery q(m_db);
        q.prepare("INSERT INTO users (username, password) VALUES (?, ?)");
        q.addBindValue(user);
        q.addBindValue(pass);
        return q.exec();
    }

    bool loginUser(const QString &user, const QString &pass)
    {
        QSqlQuery q(m_db);
        q.prepare("SELECT password FROM users WHERE username = ?");
        q.addBindValue(user);
        return q.exec() && q.next() && q.value(0).toString() == pass;
    }

    bool userExists(const QString &user)
    {
        QSqlQuery q(m_db);
        q.prepare("SELECT 1 FROM users WHERE username = ?");
        q.addBindValue(user);
        return q.exec() && q.next();
    }

    // ---- 消息 ----
    int saveMessage(const QString &sender, const QString &target,
                     const QString &type, const QString &content)
    {
        QSqlQuery q(m_db);
        q.prepare("INSERT INTO messages (sender, target, msg_type, content) VALUES (?, ?, ?, ?)");
        q.addBindValue(sender);
        q.addBindValue(target);
        q.addBindValue(type);
        q.addBindValue(content);
        if (q.exec())
            return q.lastInsertId().toInt();
        return -1;
    }

    QJsonArray getMessages(const QString &type, const QString &target,
                           int limit = 100)
    {
        QJsonArray arr;
        QSqlQuery q(m_db);
        QString sql;
        if (type == "private") {
            sql = "SELECT id, sender, target, content, timestamp, recalled FROM messages "
                  "WHERE msg_type='private' AND (sender=? OR target=?) "
                  "ORDER BY id DESC LIMIT ?";
            q.prepare(sql);
            q.addBindValue(target);
            q.addBindValue(target);
        } else if (type == "public") {
            sql = "SELECT id, sender, target, content, timestamp, recalled FROM messages "
                  "WHERE msg_type='public' ORDER BY id DESC LIMIT ?";
            q.prepare(sql);
        } else {
            sql = "SELECT id, sender, target, content, timestamp, recalled, msg_type FROM messages "
                  "ORDER BY id DESC LIMIT ?";
            q.prepare(sql);
        }
        q.addBindValue(limit);
        if (!q.exec()) return arr;

        while (q.next()) {
            QJsonObject obj;
            obj["msg_id"] = q.value(0).toInt();
            obj["sender"] = q.value(1).toString();
            obj["target"] = q.value(2).toString();
            obj["content"] = q.value(3).toString();
            obj["time"] = q.value(4).toString();
            obj["recalled"] = q.value(5).toInt();
            if (type == "all" && q.value(6).isValid())
                obj["msg_type"] = q.value(6).toString();
            arr.append(obj);
        }
        return arr;
    }

    // 按 ID 查询消息信息（撤回用）
    struct MessageInfo {
        int id;
        QString sender;
        QString target;
        QString msgType;
        QString timestamp;
    };
    MessageInfo getMessageInfo(int msgId)
    {
        MessageInfo info = {-1, "", "", "", ""};
        QSqlQuery q(m_db);
        q.prepare("SELECT id, sender, target, msg_type, timestamp FROM messages WHERE id=?");
        q.addBindValue(msgId);
        if (q.exec() && q.next()) {
            info.id = q.value(0).toInt();
            info.sender = q.value(1).toString();
            info.target = q.value(2).toString();
            info.msgType = q.value(3).toString();
            info.timestamp = q.value(4).toString();
        }
        return info;
    }

    bool markRecalled(int msgId)
    {
        QSqlQuery q(m_db);
        q.prepare("UPDATE messages SET recalled=1 WHERE id=?");
        q.addBindValue(msgId);
        return q.exec();
    }

    // 离线消息查询
    QJsonArray getMessagesSince(int lastId, const QString &username)
    {
        QJsonArray arr;
        QSqlQuery q(m_db);

        // public 消息
        QString sql = "SELECT id, sender, target, content, msg_type, timestamp, recalled "
                      "FROM messages WHERE id > ? AND ("
                      "  msg_type='public'"
                      "  OR (msg_type='private' AND (sender=? OR target=?))"
                      "  OR (msg_type='group' AND target IN ("
                      "    SELECT group_id FROM group_members WHERE username=?"
                      "  ))"
                      ") ORDER BY id ASC LIMIT 200";
        q.prepare(sql);
        q.addBindValue(lastId);
        q.addBindValue(username);
        q.addBindValue(username);
        q.addBindValue(username);
        if (!q.exec()) return arr;

        while (q.next()) {
            QJsonObject obj;
            obj["msg_id"] = q.value(0).toInt();
            obj["from"] = q.value(1).toString();
            obj["to"] = q.value(2).toString();
            obj["content"] = q.value(3).toString();
            obj["msg_type"] = q.value(4).toString();
            obj["time"] = q.value(5).toString();
            obj["recalled"] = q.value(6).toInt();
            arr.append(obj);
        }
        return arr;
    }

    // ---- 好友 ----
    bool sendFriendRequest(const QString &from, const QString &to)
    {
        QSqlQuery q(m_db);
        q.prepare("INSERT INTO friendships (requester, responder, status) VALUES (?, ?, 'pending')");
        q.addBindValue(from);
        q.addBindValue(to);
        return q.exec();
    }

    bool acceptFriendRequest(const QString &responder, const QString &requester)
    {
        QSqlQuery q(m_db);
        q.prepare("UPDATE friendships SET status='accepted' WHERE requester=? AND responder=?");
        q.addBindValue(requester);
        q.addBindValue(responder);
        return q.exec();
    }

    bool rejectFriendRequest(const QString &responder, const QString &requester)
    {
        QSqlQuery q(m_db);
        q.prepare("DELETE FROM friendships WHERE requester=? AND responder=?");
        q.addBindValue(requester);
        q.addBindValue(responder);
        return q.exec();
    }

    bool removeFriendship(const QString &user1, const QString &user2)
    {
        QSqlQuery q(m_db);
        q.prepare("DELETE FROM friendships WHERE "
                  "(requester=? AND responder=?) OR (requester=? AND responder=?)");
        q.addBindValue(user1);
        q.addBindValue(user2);
        q.addBindValue(user2);
        q.addBindValue(user1);
        return q.exec();
    }

    bool areFriends(const QString &a, const QString &b)
    {
        QSqlQuery q(m_db);
        q.prepare("SELECT 1 FROM friendships WHERE status='accepted' AND "
                  "((requester=? AND responder=?) OR (requester=? AND responder=?))");
        q.addBindValue(a); q.addBindValue(b);
        q.addBindValue(b); q.addBindValue(a);
        return q.exec() && q.next();
    }

    QStringList getFriendList(const QString &username)
    {
        QStringList list;
        QSqlQuery q(m_db);
        q.prepare("SELECT requester FROM friendships WHERE responder=? AND status='accepted' "
                  "UNION SELECT responder FROM friendships WHERE requester=? AND status='accepted'");
        q.addBindValue(username);
        q.addBindValue(username);
        if (q.exec()) {
            while (q.next())
                list.append(q.value(0).toString());
        }
        return list;
    }

    bool hasPendingRequest(const QString &from, const QString &to)
    {
        QSqlQuery q(m_db);
        q.prepare("SELECT 1 FROM friendships WHERE requester=? AND responder=? AND status='pending'");
        q.addBindValue(from);
        q.addBindValue(to);
        return q.exec() && q.next();
    }

    QStringList getPendingRequests(const QString &username)
    {
        QStringList list;
        QSqlQuery q(m_db);
        q.prepare("SELECT requester FROM friendships WHERE responder=? AND status='pending'");
        q.addBindValue(username);
        if (q.exec()) {
            while (q.next())
                list.append(q.value(0).toString());
        }
        return list;
    }

    QStringList getOutgoingRequests(const QString &username)
    {
        QStringList list;
        QSqlQuery q(m_db);
        q.prepare("SELECT responder FROM friendships WHERE requester=? AND status='pending'");
        q.addBindValue(username);
        if (q.exec()) {
            while (q.next())
                list.append(q.value(0).toString());
        }
        return list;
    }

    // ---- 群组 ----
    int createGroup(const QString &name, const QString &creator)
    {
        QSqlQuery q(m_db);
        q.prepare("INSERT INTO groups_tbl (name, creator) VALUES (?, ?)");
        q.addBindValue(name);
        q.addBindValue(creator);
        if (q.exec())
            return q.lastInsertId().toInt();
        return -1;
    }

    bool addGroupMember(int groupId, const QString &username, const QString &role = "member")
    {
        QSqlQuery q(m_db);
        q.prepare("INSERT OR IGNORE INTO group_members (group_id, username, role) VALUES (?, ?, ?)");
        q.addBindValue(groupId);
        q.addBindValue(username);
        q.addBindValue(role);
        return q.exec();
    }

    bool removeGroupMember(int groupId, const QString &username)
    {
        QSqlQuery q(m_db);
        q.prepare("DELETE FROM group_members WHERE group_id=? AND username=?");
        q.addBindValue(groupId);
        q.addBindValue(username);
        return q.exec();
    }

    bool isGroupMember(int groupId, const QString &username)
    {
        QSqlQuery q(m_db);
        q.prepare("SELECT 1 FROM group_members WHERE group_id=? AND username=?");
        q.addBindValue(groupId);
        q.addBindValue(username);
        return q.exec() && q.next();
    }

    QStringList getGroupMembers(int groupId)
    {
        QStringList list;
        QSqlQuery q(m_db);
        q.prepare("SELECT username FROM group_members WHERE group_id=?");
        q.addBindValue(groupId);
        if (q.exec()) {
            while (q.next())
                list.append(q.value(0).toString());
        }
        return list;
    }

    QJsonArray getUserGroups(const QString &username)
    {
        QJsonArray arr;
        QSqlQuery q(m_db);
        q.prepare("SELECT g.id, g.name, g.creator FROM groups_tbl g "
                  "JOIN group_members gm ON gm.group_id=g.id WHERE gm.username=?");
        q.addBindValue(username);
        if (q.exec()) {
            while (q.next()) {
                QJsonObject obj;
                obj["id"] = q.value(0).toInt();
                obj["name"] = q.value(1).toString();
                obj["creator"] = q.value(2).toString();
                arr.append(obj);
            }
        }
        return arr;
    }

    QString getGroupName(int groupId)
    {
        QSqlQuery q(m_db);
        q.prepare("SELECT name FROM groups_tbl WHERE id=?");
        q.addBindValue(groupId);
        if (q.exec() && q.next())
            return q.value(0).toString();
        return {};
    }

    // ---- 文件记录（用于历史查询） ----
    int saveFileRecord(const QString &sender, const QString &target,
                       const QString &fileType, const QString &filename,
                       const QString &filepath, qint64 size)
    {
        QSqlQuery q(m_db);
        q.prepare("INSERT INTO files (sender, target, file_type, filename, filepath, filesize) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
        q.addBindValue(sender);
        q.addBindValue(target);
        q.addBindValue(fileType);
        q.addBindValue(filename);
        q.addBindValue(filepath);
        q.addBindValue(size);
        if (q.exec())
            return q.lastInsertId().toInt();
        return -1;
    }

    QJsonArray getFiles(const QString &type, const QString &target,
                        int limit = 100)
    {
        QJsonArray arr;
        QSqlQuery q(m_db);
        QString sql;
        if (type == "private") {
            sql = "SELECT sender, target, filename, filesize, timestamp, filepath FROM files "
                  "WHERE file_type='private' AND (sender=? OR target=?) "
                  "ORDER BY id DESC LIMIT ?";
            q.prepare(sql);
            q.addBindValue(target);
            q.addBindValue(target);
        } else if (type == "public") {
            sql = "SELECT sender, target, filename, filesize, timestamp, filepath FROM files "
                  "WHERE file_type='public' ORDER BY id DESC LIMIT ?";
            q.prepare(sql);
        } else {
            sql = "SELECT sender, target, filename, filesize, timestamp, filepath, file_type FROM files "
                  "ORDER BY id DESC LIMIT ?";
            q.prepare(sql);
        }
        q.addBindValue(limit);
        if (!q.exec()) return arr;

        while (q.next()) {
            QJsonObject obj;
            obj["sender"] = q.value(0).toString();
            obj["target"] = q.value(1).toString();
            obj["filename"] = q.value(2).toString();
            obj["filesize"] = q.value(3).toInt();
            obj["time"] = q.value(4).toString();
            obj["filepath"] = q.value(5).toString();
            if (type == "all" && q.value(6).isValid())
                obj["file_type"] = q.value(6).toString();
            arr.append(obj);
        }
        return arr;
    }
};

// ============================================================
// 聊天服务器
// ============================================================
class ChatServer : public QObject
{
    Q_OBJECT
    QTcpServer  *m_server = nullptr;
    Database    *m_db = nullptr;
    QMap<QTcpSocket*, QString> m_clients;
    QMap<QString, QTcpSocket*> m_userMap;
    QSet<QString> m_onlineUsers;

    struct FileTransfer {
        QString sender;
        QString target;
        QString filename;
        QString filepath;
        int     totalChunks = 0;
        int     chunksRecv = 0;
        QFile  *file = nullptr;
    };
    QMap<int, FileTransfer> m_fileTransfers;
    int m_nextFileId = 1;
    QString m_storageDir;

public:
    ChatServer(Database *db, const QString &storageDir, QObject *parent = nullptr)
        : QObject(parent), m_db(db), m_storageDir(storageDir)
    {
        QDir().mkpath(storageDir);
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, &ChatServer::onNewConnection);
    }

    bool start(quint16 port)
    {
        if (!m_server->listen(QHostAddress::Any, port)) {
            qCritical() << "Server cannot listen on port" << port;
            return false;
        }
        qInfo() << "Chat server started on port" << port;
        return true;
    }

    void broadcastUserList()
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

    void notifyUserStatus(const QString &username, bool online)
    {
        QJsonObject msg;
        msg["type"] = MSG_USER_STATUS;
        msg["data"] = QJsonObject{{"username", username}, {"online", online}};
        QByteArray json = QJsonDocument(msg).toJson(QJsonDocument::Compact);
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
            sendPacket(it.key(), json);
    }

private slots:
    void onNewConnection()
    {
        while (m_server->hasPendingConnections()) {
            QTcpSocket *sock = m_server->nextPendingConnection();
            connect(sock, &QTcpSocket::readyRead, this, &ChatServer::onReadyRead);
            connect(sock, &QTcpSocket::disconnected, this, &ChatServer::onDisconnected);
            qInfo() << "新连接来自" << sock->peerAddress().toString();
        }
    }

    void onReadyRead()
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

    void onDisconnected()
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

    void processMessage(QTcpSocket *sock, const QByteArray &payload)
    {
        QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (!doc.isObject()) return;
        QJsonObject msg = doc.object();
        QString type = msg["type"].toString();
        QJsonObject data = msg["data"].toObject();

        QString username = m_clients.value(sock);

        if (type == MSG_REGISTER)         handleRegister(sock, data);
        else if (type == MSG_LOGIN)       handleLogin(sock, data);
        else if (type == MSG_LOGOUT)      handleLogout(sock);
        else if (type == MSG_PRIVATE_MSG) handlePrivateMsg(sock, data);
        else if (type == MSG_PUBLIC_MSG)  handlePublicMsg(sock, data);
        else if (type == MSG_FILE_META)   handleFileMeta(sock, data);
        else if (type == MSG_FILE_DATA)   handleFileData(sock, data);
        else if (type == MSG_FILE_END)    handleFileEnd(sock, data);
        else if (type == MSG_FILE_ACCEPT) handleFileAccept(sock, data);
        else if (type == MSG_HISTORY)     handleHistory(sock, data);
        else if (type == MSG_USER_LIST)   handleUserList(sock);
        else if (type == MSG_RECALL)      handleRecall(sock, data);
        else if (type == MSG_OFFLINE_QUERY) handleOfflineQuery(sock, data);
        else if (type == MSG_FRIEND_REQUEST) handleFriendRequest(sock, data);
        else if (type == MSG_FRIEND_ACCEPT)  handleFriendAccept(sock, data);
        else if (type == MSG_FRIEND_REJECT)  handleFriendReject(sock, data);
        else if (type == MSG_FRIEND_LIST)    handleFriendList(sock);
        else if (type == MSG_FRIEND_REMOVE)  handleFriendRemove(sock, data);
        else if (type == MSG_FRIEND_PENDING_LIST)  handleFriendPendingList(sock);
        else if (type == MSG_GROUP_CREATE)   handleGroupCreate(sock, data);
        else if (type == MSG_GROUP_INVITE)   handleGroupInvite(sock, data);
        else if (type == MSG_GROUP_MSG)      handleGroupMsg(sock, data);
        else if (type == MSG_GROUP_LIST)     handleGroupList(sock);
        else if (type == MSG_GROUP_LEAVE)    handleGroupLeave(sock, data);
        else if (type == MSG_GROUP_MEMBERS)  handleGroupMembers(sock, data);
        else sendError(sock, "未知消息类型");
    }

    void sendError(QTcpSocket *sock, const QString &msg)
    {
        QJsonObject res;
        res["type"] = MSG_ERROR;
        res["data"] = QJsonObject{{"message", msg}};
        sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
    }

    void sendPacket(QTcpSocket *sock, const QByteArray &json)
    {
        uint8_t header[4];
        pack32(json.size(), header);
        sock->write(reinterpret_cast<const char*>(header), 4);
        sock->write(json);
        sock->flush();
    }

    void sendJsonToUser(const QString &username, const QJsonObject &msg)
    {
        if (m_userMap.contains(username)) {
            QByteArray json = QJsonDocument(msg).toJson(QJsonDocument::Compact);
            sendPacket(m_userMap[username], json);
        }
    }

    // ---- 注册/登录 ----
    void handleRegister(QTcpSocket *sock, const QJsonObject &data)
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

    void handleLogin(QTcpSocket *sock, const QJsonObject &data)
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

    void handleLogout(QTcpSocket *sock)
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

    // ---- 私聊 ----
    void handlePrivateMsg(QTcpSocket *sock, const QJsonObject &data)
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

        // 发给接收方
        if (m_userMap.contains(target))
            sendPacket(m_userMap[target], json);

        // 回显给发送方
        sendPacket(sock, json);
    }

    // ---- 公共消息 ----
    void handlePublicMsg(QTcpSocket *sock, const QJsonObject &data)
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

    // ---- 消息撤回 ----
    void handleRecall(QTcpSocket *sock, const QJsonObject &data)
    {
        QString username = m_clients.value(sock);
        if (username.isEmpty()) return;

        int msgId = data["msg_id"].toInt();
        auto info = m_db->getMessageInfo(msgId);
        if (info.id < 0) {
            sendError(sock, "消息不存在");
            return;
        }

        // 只有发送者能撤回
        if (info.sender != username) {
            sendError(sock, "只能撤回自己的消息");
            return;
        }

        // 2 分钟限制
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

        // 通知相关方
        QJsonObject ntf;
        ntf["type"] = MSG_RECALL_NTF;
        ntf["data"] = QJsonObject{
            {"msg_id", msgId},
            {"from", username},
            {"target", info.target},
            {"msg_type", info.msgType}
        };

        if (info.msgType == "public") {
            QByteArray json = QJsonDocument(ntf).toJson(QJsonDocument::Compact);
            for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
                sendPacket(it.key(), json);
        } else if (info.msgType == "private") {
            QString other = (info.target == username) ? info.sender : info.target;
            // 发给对方
            if (m_userMap.contains(other))
                sendPacket(m_userMap[other], QJsonDocument(ntf).toJson(QJsonDocument::Compact));
            // 也回给自己（用于确认）
            sendPacket(sock, QJsonDocument(ntf).toJson(QJsonDocument::Compact));
        } else if (info.msgType == "group") {
            // 发给群成员
            int groupId = info.target.toInt();
            QStringList members = m_db->getGroupMembers(groupId);
            QByteArray json = QJsonDocument(ntf).toJson(QJsonDocument::Compact);
            for (const auto &m : members) {
                if (m_userMap.contains(m))
                    sendPacket(m_userMap[m], json);
            }
        }
    }

    // ---- 离线消息 ----
    void handleOfflineQuery(QTcpSocket *sock, const QJsonObject &data)
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

    // ---- 好友系统 ----
    void handleFriendRequest(QTcpSocket *sock, const QJsonObject &data)
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
            // 对方已经发过请求 → 自动接受
            m_db->acceptFriendRequest(from, target);
            res["data"] = QJsonObject{{"ok", true}, {"message", "对方已向您发送过请求，已自动成为好友"}};
            sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));

            // 通知双方
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

            // 通知目标用户
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

    void handleFriendAccept(QTcpSocket *sock, const QJsonObject &data)
    {
        QString username = m_clients.value(sock);
        if (username.isEmpty()) return;

        QString requester = data["from"].toString();

        QJsonObject res;
        res["type"] = MSG_FRIEND_ACCEPT_RES;

        if (m_db->acceptFriendRequest(username, requester)) {
            res["data"] = QJsonObject{{"ok", true}};
            sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));

            // 通知请求方
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

    void handleFriendReject(QTcpSocket *sock, const QJsonObject &data)
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

    void handleFriendList(QTcpSocket *sock)
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

    void handleFriendRemove(QTcpSocket *sock, const QJsonObject &data)
    {
        QString username = m_clients.value(sock);
        if (username.isEmpty()) return;

        QString target = data["target"].toString();
        QJsonObject res;
        res["type"] = MSG_FRIEND_REMOVE_RES;
        res["data"] = QJsonObject{{"ok", m_db->removeFriendship(username, target)}};
        sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
    }

    void handleFriendPendingList(QTcpSocket *sock)
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

    // ---- 群聊 ----
    void handleGroupCreate(QTcpSocket *sock, const QJsonObject &data)
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

        // 添加创建者为群主
        m_db->addGroupMember(groupId, creator, "owner");

        // 添加初始成员
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

        // 通知群成员
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

    void handleGroupMsg(QTcpSocket *sock, const QJsonObject &data)
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

        // 转发给所有在线群成员
        QStringList members = m_db->getGroupMembers(groupId);
        QByteArray json = QJsonDocument(fwd).toJson(QJsonDocument::Compact);
        for (const auto &m : members) {
            if (m_userMap.contains(m))
                sendPacket(m_userMap[m], json);
        }
    }

    void handleGroupList(QTcpSocket *sock)
    {
        QString username = m_clients.value(sock);
        if (username.isEmpty()) return;

        QJsonObject res;
        res["type"] = MSG_GROUP_LIST_RES;
        res["data"] = QJsonObject{{"groups", m_db->getUserGroups(username)}};
        sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
    }

    void handleGroupLeave(QTcpSocket *sock, const QJsonObject &data)
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

    void handleGroupInvite(QTcpSocket *sock, const QJsonObject &data)
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

    void handleGroupMembers(QTcpSocket *sock, const QJsonObject &data)
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

    // ---- 文件 ----
    void handleFileMeta(QTcpSocket *sock, const QJsonObject &data)
    {
        QString sender = m_clients.value(sock);
        if (sender.isEmpty()) return;

        QString target = data["target"].toString();
        QString filename = data["filename"].toString();
        qint64  filesize = data["filesize"].toVariant().toLongLong();
        QString fileType = data["file_type"].toString();

        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString safeName = timestamp + "_" + filename;
        QString filepath = m_storageDir + "/" + safeName;

        int fileId = m_nextFileId++;

        FileTransfer ft;
        ft.sender = sender;
        ft.target = target;
        ft.filename = filename;
        ft.filepath = filepath;
        ft.file = new QFile(filepath);
        if (!ft.file->open(QIODevice::WriteOnly)) {
            delete ft.file;
            sendError(sock, "服务器无法创建文件");
            return;
        }
        m_fileTransfers.insert(fileId, ft);

        m_db->saveFileRecord(sender, target, fileType, filename, filepath, filesize);

        QJsonObject res;
        res["type"] = MSG_FILE_META_RES;
        res["data"] = QJsonObject{{"file_id", fileId}, {"ok", true}};
        sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));

        if (fileType == "private" && m_userMap.contains(target)) {
            QJsonObject notify;
            notify["type"] = MSG_FILE_INCOMING;
            notify["data"] = QJsonObject{
                {"file_id", fileId},
                {"from", sender},
                {"filename", filename},
                {"filesize", filesize}
            };
            sendPacket(m_userMap[target], QJsonDocument(notify).toJson(QJsonDocument::Compact));
        }
    }

    void handleFileData(QTcpSocket *sock, const QJsonObject &data)
    {
        QString sender = m_clients.value(sock);
        if (sender.isEmpty()) return;

        int fileId = data["file_id"].toInt();
        if (!m_fileTransfers.contains(fileId)) return;

        FileTransfer &ft = m_fileTransfers[fileId];
        QString chunkStr = data["data"].toString();
        QByteArray chunk = QByteArray::fromBase64(chunkStr.toUtf8());
        ft.file->write(chunk);
        ft.chunksRecv++;

        if (ft.target != "ALL" && m_userMap.contains(ft.target)) {
            QJsonObject fwd;
            fwd["type"] = MSG_FILE_DATA_FWD;
            fwd["data"] = QJsonObject{
                {"file_id", fileId},
                {"seq", data["seq"].toInt()},
                {"data", chunkStr}
            };
            sendPacket(m_userMap[ft.target], QJsonDocument(fwd).toJson(QJsonDocument::Compact));
        }
    }

    void handleFileEnd(QTcpSocket *sock, const QJsonObject &data)
    {
        QString sender = m_clients.value(sock);
        if (sender.isEmpty()) return;

        int fileId = data["file_id"].toInt();
        if (!m_fileTransfers.contains(fileId)) return;

        FileTransfer &ft = m_fileTransfers[fileId];
        ft.file->close();
        delete ft.file;
        ft.file = nullptr;

        QString fileType = (ft.target == "ALL") ? "public" : "private";

        if (ft.target != "ALL" && m_userMap.contains(ft.target)) {
            QJsonObject fwd;
            fwd["type"] = MSG_FILE_END_FWD;
            fwd["data"] = QJsonObject{
                {"file_id", fileId},
                {"from", sender},
                {"filename", ft.filename},
                {"filepath", ft.filepath}
            };
            sendPacket(m_userMap[ft.target], QJsonDocument(fwd).toJson(QJsonDocument::Compact));
        }

        qInfo() << "文件传输完成：" << ft.filename
                << "从" << sender << "到" << ft.target;

        m_fileTransfers.remove(fileId);
    }

    void handleFileAccept(QTcpSocket *sock, const QJsonObject &data)
    {
        int fileId = data["file_id"].toInt();
        QString sender = m_clients.value(sock);
        if (sender.isEmpty()) return;

        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (it.value() == data["target"].toString()) {
                QJsonObject fwd;
                fwd["type"] = MSG_FILE_ACCEPT;
                fwd["data"] = QJsonObject{{"file_id", fileId}, {"accepted", true}};
                sendPacket(it.key(), QJsonDocument(fwd).toJson(QJsonDocument::Compact));
                break;
            }
        }
    }

    void handleHistory(QTcpSocket *sock, const QJsonObject &data)
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

    void handleUserList(QTcpSocket *sock)
    {
        QJsonObject res;
        res["type"] = MSG_USER_LIST_RES;
        QJsonArray users;
        for (const auto &u : m_onlineUsers)
            users.append(u);
        res["data"] = QJsonObject{{"users", users}};
        sendPacket(sock, QJsonDocument(res).toJson(QJsonDocument::Compact));
    }

private:
    QMap<QTcpSocket*, QByteArray> m_recvBuf;
};

// ============================================================
// 主函数
// ============================================================
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("chat-server");

    bool foreground = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--foreground") == 0 || strcmp(argv[i], "-f") == 0)
            foreground = true;
    }
    if (!foreground) daemonize();

    Database db;
    if (!db.open("chat_server.db")) {
        qCritical("无法打开数据库");
        return 1;
    }

    QString storageDir = "chat_files";
    QDir().mkpath(storageDir);

    ChatServer server(&db, storageDir);
    if (!server.start(SERVER_PORT))
        return 1;

    qInfo() << "Chat server running (foreground:" << foreground << ")";
    return app.exec();
}

#include "server.moc"

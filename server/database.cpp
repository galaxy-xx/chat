#include "database.h"

bool Database::open(const QString &path)
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

void Database::initTables()
{
    QSqlQuery q(m_db);
    q.exec("CREATE TABLE IF NOT EXISTS users ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "username TEXT UNIQUE NOT NULL,"
           "password TEXT NOT NULL,"
           "created_at TEXT DEFAULT (datetime('now','localtime'))"
           ")");
    q.exec("CREATE TABLE IF NOT EXISTS messages ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "sender TEXT NOT NULL,"
           "target TEXT NOT NULL,"
           "msg_type TEXT NOT NULL,"
           "content TEXT NOT NULL,"
           "recalled INTEGER DEFAULT 0,"
           "timestamp TEXT DEFAULT (datetime('now','localtime'))"
           ")");
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
    q.exec("CREATE TABLE IF NOT EXISTS friendships ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "requester TEXT NOT NULL,"
           "responder TEXT NOT NULL,"
           "status TEXT DEFAULT 'pending',"
           "created_at TEXT DEFAULT (datetime('now','localtime')),"
           "UNIQUE(requester, responder)"
           ")");
    q.exec("CREATE TABLE IF NOT EXISTS groups_tbl ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "name TEXT NOT NULL,"
           "creator TEXT NOT NULL,"
           "created_at TEXT DEFAULT (datetime('now','localtime'))"
           ")");
    q.exec("CREATE TABLE IF NOT EXISTS group_members ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "group_id INTEGER NOT NULL,"
           "username TEXT NOT NULL,"
           "role TEXT DEFAULT 'member',"
           "joined_at TEXT DEFAULT (datetime('now','localtime')),"
           "UNIQUE(group_id, username)"
           ")");
    q.exec("ALTER TABLE messages ADD COLUMN recalled INTEGER DEFAULT 0");
    qInfo("Database tables initialized");
}

bool Database::registerUser(const QString &user, const QString &pass)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO users (username, password) VALUES (?, ?)");
    q.addBindValue(user);
    q.addBindValue(pass);
    return q.exec();
}

bool Database::loginUser(const QString &user, const QString &pass)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT password FROM users WHERE username = ?");
    q.addBindValue(user);
    return q.exec() && q.next() && q.value(0).toString() == pass;
}

bool Database::userExists(const QString &user)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM users WHERE username = ?");
    q.addBindValue(user);
    return q.exec() && q.next();
}

int Database::saveMessage(const QString &sender, const QString &target,
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

QJsonArray Database::getMessages(const QString &type, const QString &target,
                                  int limit)
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

Database::MessageInfo Database::getMessageInfo(int msgId)
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

bool Database::markRecalled(int msgId)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE messages SET recalled=1 WHERE id=?");
    q.addBindValue(msgId);
    return q.exec();
}

QJsonArray Database::getMessagesSince(int lastId, const QString &username)
{
    QJsonArray arr;
    QSqlQuery q(m_db);
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

bool Database::sendFriendRequest(const QString &from, const QString &to)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO friendships (requester, responder, status) VALUES (?, ?, 'pending')");
    q.addBindValue(from);
    q.addBindValue(to);
    return q.exec();
}

bool Database::acceptFriendRequest(const QString &responder, const QString &requester)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE friendships SET status='accepted' WHERE requester=? AND responder=?");
    q.addBindValue(requester);
    q.addBindValue(responder);
    return q.exec();
}

bool Database::rejectFriendRequest(const QString &responder, const QString &requester)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM friendships WHERE requester=? AND responder=?");
    q.addBindValue(requester);
    q.addBindValue(responder);
    return q.exec();
}

bool Database::removeFriendship(const QString &user1, const QString &user2)
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

bool Database::areFriends(const QString &a, const QString &b)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM friendships WHERE status='accepted' AND "
              "((requester=? AND responder=?) OR (requester=? AND responder=?))");
    q.addBindValue(a); q.addBindValue(b);
    q.addBindValue(b); q.addBindValue(a);
    return q.exec() && q.next();
}

QStringList Database::getFriendList(const QString &username)
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

bool Database::hasPendingRequest(const QString &from, const QString &to)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM friendships WHERE requester=? AND responder=? AND status='pending'");
    q.addBindValue(from);
    q.addBindValue(to);
    return q.exec() && q.next();
}

QStringList Database::getPendingRequests(const QString &username)
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

QStringList Database::getOutgoingRequests(const QString &username)
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

int Database::createGroup(const QString &name, const QString &creator)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO groups_tbl (name, creator) VALUES (?, ?)");
    q.addBindValue(name);
    q.addBindValue(creator);
    if (q.exec())
        return q.lastInsertId().toInt();
    return -1;
}

bool Database::addGroupMember(int groupId, const QString &username, const QString &role)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO group_members (group_id, username, role) VALUES (?, ?, ?)");
    q.addBindValue(groupId);
    q.addBindValue(username);
    q.addBindValue(role);
    return q.exec();
}

bool Database::removeGroupMember(int groupId, const QString &username)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM group_members WHERE group_id=? AND username=?");
    q.addBindValue(groupId);
    q.addBindValue(username);
    return q.exec();
}

bool Database::isGroupMember(int groupId, const QString &username)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM group_members WHERE group_id=? AND username=?");
    q.addBindValue(groupId);
    q.addBindValue(username);
    return q.exec() && q.next();
}

QStringList Database::getGroupMembers(int groupId)
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

QJsonArray Database::getUserGroups(const QString &username)
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

QString Database::getGroupName(int groupId)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT name FROM groups_tbl WHERE id=?");
    q.addBindValue(groupId);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return {};
}

int Database::saveFileRecord(const QString &sender, const QString &target,
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

QJsonArray Database::getFiles(const QString &type, const QString &target,
                               int limit)
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

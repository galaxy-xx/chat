#include "../server.h"
#include "../database.h"

void ChatServer::handleFileMeta(QTcpSocket *sock, const QJsonObject &data)
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

void ChatServer::handleFileData(QTcpSocket *sock, const QJsonObject &data)
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

void ChatServer::handleFileEnd(QTcpSocket *sock, const QJsonObject &data)
{
    QString sender = m_clients.value(sock);
    if (sender.isEmpty()) return;

    int fileId = data["file_id"].toInt();
    if (!m_fileTransfers.contains(fileId)) return;

    FileTransfer &ft = m_fileTransfers[fileId];
    ft.file->close();
    delete ft.file;
    ft.file = nullptr;

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

void ChatServer::handleFileAccept(QTcpSocket *sock, const QJsonObject &data)
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QLabel>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QSet>
#include <QFile>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QFrame>
#include <QStackedWidget>

#include "clientnetwork.h"

// 单个聊天气泡组件
class BubbleWidget : public QFrame
{
    Q_OBJECT
public:
    explicit BubbleWidget(const QString &content, const QString &time,
                          bool isSelf, int msgId = -1,
                          QWidget *parent = nullptr);
    int msgId() const { return m_msgId; }
    bool isSelf() const { return m_isSelf; }
    void markRecalled();

private:
    QLabel *m_bubbleLabel = nullptr;
    int m_msgId;
    bool m_isSelf;
};

// 聊天区域组件（滚动区 + 气泡列表）
class ChatWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChatWidget(const QString &chatWith, QWidget *parent = nullptr);

    void appendMessage(const QString &from, const QString &content,
                       const QString &time, int msgId = -1);
    void appendSystemMessage(const QString &msg);
    void appendImageMessage(const QString &from, const QString &filepath,
                            const QString &filename, const QString &time);
    bool removeMessage(int msgId);
    QString chatWith() const { return m_chatWith; }

signals:
    void imageClicked(const QString &filepath);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QScrollArea *m_scrollArea;
    QWidget     *m_contentWidget;
    QVBoxLayout *m_contentLayout;
    QString m_chatWith;
    QMap<int, BubbleWidget*> m_msgMap; // msgId -> BubbleWidget
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(ClientNetwork *net, const QString &username,
                        QWidget *parent = nullptr);
    ~MainWindow() override;

    int lastMsgId() const { return m_lastMsgId; }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onMessageReceived(const QJsonObject &msg);
    void onSendClicked();
    void onSendFile();
    void onShowHistory();
    void onDisconnect();

private:
    void setupUI();
    void updateUserList(const QJsonArray &users);
    void appendPublicMessage(const QString &from, const QString &content,
                             const QString &time, int msgId = -1);
    void appendPrivateMessage(const QString &from, const QString &content,
                              const QString &time, const QString &target, int msgId = -1);
    void openPrivateChat(const QString &user);
    ChatWidget* getOrCreatePrivateChat(const QString &user);
    void showFileDialog(bool isPrivate);
    void showHistoryDialog();
    void showIncomingFileDialog(int fileId, const QString &from,
                                const QString &filename, qint64 filesize);
    void showHistoryResults(const QJsonObject &data);

    // 图片预览
    bool isImageFile(const QString &filename);
    void showImagePreview(const QString &filepath);

    // 消息撤回
    void handleRecallNtf(const QJsonObject &data);
    void showRecallMenu(BubbleWidget *bubble);

    // 离线消息
    void requestOfflineMessages();

    // 好友系统
    QStringList m_friends;
    QStringList m_onlineUsers;
    QStringList m_pendingIncoming;
    QStringList m_pendingOutgoing;
    void onAddFriend();
    void onFriendManagement();
    void onFriendRequestIncoming(const QString &from);
    void onFriendRequestSent(bool ok, const QString &msg);
    void onFriendAccepted(const QString &by);
    void refreshFriendList();
    void rebuildGroupList();
    void rebuildContactList();

    // 群聊
    QMap<int, ChatWidget*> m_groupChats;
    QMap<int, QString>     m_groupNames;
    void onCreateGroup();
    void onGroupMembers(const QJsonObject &data);
    void onGroupInviteRes(const QJsonObject &data);
    void showGroupMembers(int groupId);
    void inviteToGroup(int groupId);
    void leaveGroup(int groupId);
    ChatWidget* getOrCreateGroupChat(int groupId, const QString &groupName);
    void openGroupChat(int groupId);

    // 未读消息
    QMap<QString, int> m_unreadPrivate;
    QMap<int, int>     m_unreadGroup;
    int m_unreadPublic = 0;
    void incUnread(const QString &key, bool isGroup = false);
    void clearUnread(const QString &key, bool isGroup = false);
    void updateGroupBadge(int groupId, int count);
    void updateContactBadge(const QString &key, int count);

    // 界面组件
    ClientNetwork *m_net;
    QString m_username;
    int m_lastMsgId = 0;

    QLabel      *m_selfLabel;
    QListWidget *m_groupList;   // 群组列表
    QListWidget *m_contactList; // 联系人列表（好友+在线用户+公共聊天）
    QLabel      *m_chatHeader;
    QStackedWidget *m_chatStack;
    QTextEdit   *m_inputEdit;
    QPushButton *m_sendBtn;

    ChatWidget *m_publicChat = nullptr;
    QMap<QString, ChatWidget*> m_privateChats;

    struct FileDownload {
        QString from;
        QString filename;
        QString filepath;
        QFile  *file = nullptr;
        int     chunksRecv = 0;
    };
    QMap<int, FileDownload> m_downloads;
};

#endif // MAINWINDOW_H

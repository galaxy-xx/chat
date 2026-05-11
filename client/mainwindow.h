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
#include "widgets/chat.h"

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
    void showIncomingFileDialog(int fileId, const QString &from,
                                const QString &filename, qint64 filesize);
    void showHistoryDialog();

    // 图片预览
    static bool isImageFile(const QString &filename);

    // 消息撤回
    void handleRecallNtf(const QJsonObject &data);
    void showRecallMenu(class BubbleWidget *bubble);

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
    QListWidget *m_groupList;
    QListWidget *m_contactList;
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

#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QLabel>
#include <QScrollBar>
#include <QTimer>
#include <QComboBox>
#include <QStackedWidget>
#include <QGraphicsDropShadowEffect>
#include <QDesktopServices>
#include <QUrl>
#include <QPixmap>
#include <QListWidgetItem>
#include <QClipboard>
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>

#include "widgets/bubble.h"
#include "dialogs/frienddlg.h"
#include "dialogs/groupcreatedlg.h"
#include "dialogs/memberdlg.h"
#include "dialogs/historydlg.h"
#include "dialogs/imagedlg.h"

// ============================================================
// MainWindow 实现
// ============================================================
MainWindow::MainWindow(ClientNetwork *net, const QString &username,
                       QWidget *parent)
    : QMainWindow(parent), m_net(net), m_username(username)
{
    // 读取上次已看过的消息 ID
    QString statePath = QDir::homePath()
                        + QStringLiteral("/.chat_state_%1.json").arg(username);
    QFile f(statePath);
    if (f.open(QIODevice::ReadOnly)) {
        QJsonObject state = QJsonDocument::fromJson(f.readAll()).object();
        m_lastMsgId = state["lastMsgId"].toInt();
        f.close();
    }

    setupUI();
    setWindowTitle(QStringLiteral("微信"));
    setMinimumSize(800, 560);
    resize(920, 620);

    connect(m_net, &ClientNetwork::messageReceived,
            this, &MainWindow::onMessageReceived);

    QTimer::singleShot(500, this, [this]() {
        m_net->sendFriendList();
        m_net->sendGroupList();
        requestOfflineMessages();
    });
}

MainWindow::~MainWindow()
{
    QJsonObject state;
    state["lastMsgId"] = m_lastMsgId;
    QString statePath = QDir::homePath()
                        + QStringLiteral("/.chat_state_%1.json").arg(m_username);
    QFile f(statePath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
        f.close();
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_inputEdit && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (!(keyEvent->modifiers() & Qt::ShiftModifier)) {
                onSendClicked();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setupUI()
{
    auto *menuBar = new QMenuBar(this);

    auto *fileMenu = menuBar->addMenu(QStringLiteral("文件"));
    fileMenu->addAction(QStringLiteral("发送文件..."), this, [this](){ showFileDialog(); });
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("添加好友..."), this, &MainWindow::onAddFriend);
    fileMenu->addAction(QStringLiteral("好友管理..."), this, &MainWindow::onFriendManagement);
    fileMenu->addAction(QStringLiteral("创建群聊..."), this, &MainWindow::onCreateGroup);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("注销账号"), this, &MainWindow::onDeleteAccount);
    fileMenu->addAction(QStringLiteral("退出登录"), this, &MainWindow::onDisconnect);

    auto *viewMenu = menuBar->addMenu(QStringLiteral("查看"));
    viewMenu->addAction(QStringLiteral("聊天历史..."), this, [this](){ showHistoryDialog(); });
    setMenuBar(menuBar);

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 左侧面板
    auto *leftPanel = new QWidget(this);
    leftPanel->setFixedWidth(260);
    leftPanel->setStyleSheet("background: #2E3238;");

    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    auto *headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background: #07C160; padding: 10px;");
    auto *headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(15, 10, 15, 15);

    m_selfLabel = new QLabel(QStringLiteral("%1").arg(m_username), this);
    m_selfLabel->setStyleSheet(
        "font-size: 16px; font-weight: bold; color: white; background: transparent;");
    headerLayout->addWidget(m_selfLabel);

    auto *subLabel = new QLabel(QStringLiteral("聊天"), this);
    subLabel->setStyleSheet("font-size: 12px; color: rgba(255,255,255,0.7); background: transparent;");
    headerLayout->addWidget(subLabel);

    leftLayout->addWidget(headerWidget);

    auto *searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText(QStringLiteral("搜索联系人..."));
    searchEdit->setStyleSheet(
        "QLineEdit {"
        "  background: #3A3F45; border: none; border-radius: 4px;"
        "  padding: 6px 10px; margin: 6px 10px;"
        "  font-size: 13px; color: #CCCCCC;"
        "}"
        "QLineEdit:focus { background: #454A52; }"
        "QLineEdit::placeholder { color: #666666; }");
    leftLayout->addWidget(searchEdit);

    auto *groupHeader = new QLabel(QStringLiteral(" 群组"), this);
    groupHeader->setStyleSheet(
        "color: #888888; font-size: 12px; padding: 6px 12px 2px 12px;"
        "background: #2E3238; border: none;");
    leftLayout->addWidget(groupHeader);

    m_groupList = new QListWidget(this);
    m_groupList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_groupList->setMaximumHeight(180);
    m_groupList->setStyleSheet(
        "QListWidget { background: #2E3238; border: none; font-size: 14px; outline: none; }"
        "QListWidget::item {"
        "  color: white; padding: 8px 18px;"
        "  border-bottom: 1px solid #3A3F45;"
        "}"
        "QListWidget::item:hover { background: #3A3F45; }"
        "QListWidget::item:selected { background: #4A4F55; }");
    leftLayout->addWidget(m_groupList);

    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #3A3F45; margin: 2px 10px;");
    leftLayout->addWidget(sep);

    auto *contactHeader = new QLabel(QStringLiteral(" 联系人"), this);
    contactHeader->setStyleSheet(
        "color: #888888; font-size: 12px; padding: 2px 12px 2px 12px;"
        "background: #2E3238; border: none;");
    leftLayout->addWidget(contactHeader);

    m_contactList = new QListWidget(this);
    m_contactList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_contactList->setStyleSheet(
        "QListWidget { background: #2E3238; border: none; font-size: 14px; outline: none; }"
        "QListWidget::item {"
        "  color: white; padding: 8px 18px;"
        "  border-bottom: 1px solid #3A3F45;"
        "}"
        "QListWidget::item:hover { background: #3A3F45; }"
        "QListWidget::item:selected { background: #4A4F55; }");
    leftLayout->addWidget(m_contactList, 1);

    mainLayout->addWidget(leftPanel);

    // 右侧面板
    auto *rightPanel = new QWidget(this);
    rightPanel->setStyleSheet("background: #F7F7F7;");
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_chatHeader = new QLabel(QStringLiteral("公共聊天"), this);
    m_chatHeader->setFixedHeight(48);
    m_chatHeader->setAlignment(Qt::AlignCenter);
    m_chatHeader->setStyleSheet(
        "background: #F7F7F7; font-size: 15px; font-weight: bold;"
        "color: #353535; border-bottom: 1px solid #EBEBEB;");
    rightLayout->addWidget(m_chatHeader);

    m_chatStack = new QStackedWidget(this);
    m_chatStack->setStyleSheet("background: #EDEDED;");

    m_publicChat = new ChatWidget("ALL", this);
    m_publicChat->appendSystemMessage(QStringLiteral("欢迎进入公共聊天室"));
    m_chatStack->addWidget(m_publicChat);
    connect(m_publicChat, &ChatWidget::bubbleRightClicked,
            this, &MainWindow::showRecallMenu);
    connect(m_publicChat, &ChatWidget::imageClicked,
            this, [this](const QString &fp) { ImagePreviewDialog::show(fp, this); });

    rightLayout->addWidget(m_chatStack, 1);

    // 底部输入
    auto *bottomContainer = new QWidget(this);
    bottomContainer->setStyleSheet("background: #F7F7F7; border-top: 1px solid #EBEBEB;");
    auto *bottomLayout = new QVBoxLayout(bottomContainer);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(0);

    auto *toolBar = new QWidget(this);
    toolBar->setStyleSheet("background: #F7F7F7;");
    auto *toolLayout = new QHBoxLayout(toolBar);
    toolLayout->setContentsMargins(8, 4, 8, 2);
    toolLayout->setSpacing(4);

    auto *attachBtn = new QPushButton(QStringLiteral("选择文件"), this);
    attachBtn->setCursor(Qt::PointingHandCursor);
    attachBtn->setToolTip(QStringLiteral("发送文件"));
    attachBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #07C160; border: 1px solid #07C160;"
        "  border-radius: 4px; font-size: 12px; padding: 4px 8px; }"
        "QPushButton:hover { background: #E8F8EE; }");

    toolLayout->addWidget(attachBtn);
    toolLayout->addStretch();

    auto *hintLabel = new QLabel(QStringLiteral("按 Enter 发送"), this);
    hintLabel->setStyleSheet("color: #B0B0B0; font-size: 11px; background: transparent;");
    toolLayout->addWidget(hintLabel);

    bottomLayout->addWidget(toolBar);

    auto *inputContainer = new QWidget(this);
    inputContainer->setStyleSheet("background: #F7F7F7;");
    auto *inputLayout = new QHBoxLayout(inputContainer);
    inputLayout->setContentsMargins(10, 2, 10, 8);

    m_inputEdit = new QTextEdit(this);
    m_inputEdit->setMaximumHeight(72);
    m_inputEdit->setPlaceholderText(QStringLiteral("输入消息..."));
    m_inputEdit->installEventFilter(this);
    m_inputEdit->setStyleSheet(
        "QTextEdit {"
        "  border: 1px solid #D9D9D9; border-radius: 4px;"
        "  padding: 7px 10px; font-size: 14px;"
        "  background: white; color: #353535;"
        "}"
        "QTextEdit:focus { border-color: #07C160; }");

    m_sendBtn = new QPushButton(QStringLiteral("发送"), this);
    m_sendBtn->setFixedWidth(72);
    m_sendBtn->setFixedHeight(36);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setStyleSheet(
        "QPushButton {"
        "  background: #07C160; color: white; border: none;"
        "  border-radius: 4px; font-size: 14px;"
        "}"
        "QPushButton:hover { background: #06AD56; }"
        "QPushButton:disabled { background: #BBBBBB; }");

    inputLayout->addWidget(m_inputEdit, 1);
    inputLayout->addWidget(m_sendBtn);
    bottomLayout->addWidget(inputContainer);

    rightLayout->addWidget(bottomContainer);

    mainLayout->addWidget(rightPanel, 1);

    setCentralWidget(centralWidget);

    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(attachBtn, &QPushButton::clicked, this, [this](){ showFileDialog(); });

    connect(m_groupList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *item) {
        QString role = item->data(Qt::UserRole).toString();
        if (role.startsWith("group:"))
            openGroupChat(role.mid(6).toInt());
    });

    connect(m_groupList, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        QListWidgetItem *item = m_groupList->itemAt(pos);
        if (!item) return;
        QString text = item->data(Qt::UserRole).toString();
        if (!text.startsWith("group:")) return;
        int groupId = text.mid(6).toInt();

        QMenu menu;
        menu.addAction(QStringLiteral("发送消息"), this, [this, groupId]() {
            openGroupChat(groupId);
        });
        menu.addAction(QStringLiteral("查看群成员"), this, [this, groupId]() {
            showGroupMembers(groupId);
        });
        menu.addAction(QStringLiteral("邀请好友"), this, [this, groupId]() {
            inviteToGroup(groupId);
        });
        menu.addAction(QStringLiteral("退出群聊"), this, [this, groupId]() {
            leaveGroup(groupId);
        });
        menu.exec(m_groupList->mapToGlobal(pos));
    });

    connect(m_contactList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *item) {
        QString role = item->data(Qt::UserRole).toString();
        if (role == "separator" || role == "header" || role == "online_header") return;
        if (role == "public") {
            m_chatStack->setCurrentWidget(m_publicChat);
            m_chatHeader->setText(QStringLiteral("公共聊天"));
            clearUnread("public");
        } else if (!role.isEmpty()) {
            openPrivateChat(role);
        }
    });

    connect(m_contactList, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        QListWidgetItem *item = m_contactList->itemAt(pos);
        if (!item) return;
        QString text = item->data(Qt::UserRole).toString();
        if (text == "separator" || text == "header" || text == "online_header" || text == "public")
            return;

        QMenu menu;
        menu.addAction(QStringLiteral("发送私聊"), this, [this, text]() {
            openPrivateChat(text);
        });
        if (m_friends.contains(text)) {
            menu.addAction(QStringLiteral("删除好友"), this, [this, text]() {
                m_net->sendFriendRemove(text);
                m_friends.removeAll(text);
                rebuildGroupList();
                rebuildContactList();
            });
        } else {
            menu.addAction(QStringLiteral("添加好友"), this, [this, text]() {
                m_net->sendFriendRequest(text);
            });
        }
        menu.exec(m_contactList->mapToGlobal(pos));
    });
}

void MainWindow::onMessageReceived(const QJsonObject &msg)
{
    QString type = msg["type"].toString();
    QJsonObject data = msg["data"].toObject();

    if (type == MSG_USER_LIST_RES) {
        updateUserList(data["users"].toArray());
    }
    else if (type == MSG_MESSAGE) {
        QString from = data["from"].toString();
        QString to = data["to"].toString();
        QString content = data["content"].toString();
        QString time = data["time"].toString();
        QString msgType = data["msg_type"].toString();
        int msgId = data["msg_id"].toInt();

        if (msgId > 0 && msgId > m_lastMsgId)
            m_lastMsgId = msgId;

        if (from == m_username) {
            // 收到自己消息的回显，更新最后一条消息的 msgId
            if (msgType == "public")
                m_publicChat->updateLastMsgId(msgId);
            else {
                QString partner = to;
                if (m_privateChats.contains(partner))
                    m_privateChats[partner]->updateLastMsgId(msgId);
            }
            return;
        }

        if (msgType == "public") {
            appendPublicMessage(from, content, time, msgId);
            if (m_chatStack->currentWidget() != m_publicChat)
                incUnread("public");
        } else {
            appendPrivateMessage(from, content, time, to, msgId);
        }
    }
    else if (type == MSG_FILE_MSG) {
        QString from = data["from"].toString();
        QString to = data["to"].toString();
        QString filename = data["filename"].toString();
        qint64 filesize = data["filesize"].toVariant().toLongLong();
        QString time = data["time"].toString();
        QString base64Data = data["data"].toString();

        if (from == m_username) return;

        ChatWidget *chat = (to == "ALL") ? m_publicChat
                                         : m_privateChats.value(to.isEmpty() ? from : to);
        if (!chat) chat = getOrCreatePrivateChat(from);
        handleReceivedFile(from, filename, filesize, base64Data, time, chat);
    }
    else if (type == MSG_GROUP_FILE_MSG) {
        QString from = data["from"].toString();
        int groupId = data["group_id"].toInt();
        QString filename = data["filename"].toString();
        qint64 filesize = data["filesize"].toVariant().toLongLong();
        QString time = data["time"].toString();
        QString base64Data = data["data"].toString();

        if (from == m_username) return;

        ChatWidget *chat = m_groupChats.value(groupId);
        if (chat)
            handleReceivedFile(from, filename, filesize, base64Data, time, chat);
    }
    else if (type == MSG_ERROR) {
        QMessageBox::warning(this, QStringLiteral("错误"), data["message"].toString());
    }
    // 消息撤回
    else if (type == MSG_RECALL_RES) {
        bool ok = data["ok"].toBool();
        if (!ok) QMessageBox::information(this, QStringLiteral("撤回"), data["message"].toString());
    }
    else if (type == MSG_RECALL_NTF) {
        handleRecallNtf(data);
    }
    // 离线消息
    else if (type == MSG_OFFLINE_RES) {
        QJsonArray messages = data["messages"].toArray();
        int seenThreshold = m_lastMsgId;

        for (const auto &m : messages) {
            QJsonObject obj = m.toObject();
            int msgId = obj["msg_id"].toInt();
            if (msgId > m_lastMsgId) m_lastMsgId = msgId;

            QString from = obj["from"].toString();
            QString content = obj["content"].toString();
            QString time = obj["time"].toString();
            QString msgType = obj["msg_type"].toString();
            int recalled = obj["recalled"].toInt();

            if (recalled) continue;
            bool isNew = (msgId > seenThreshold);

            if (msgType == "public") {
                QString displayFrom = (from == m_username) ? QStringLiteral("我") : from;
                m_publicChat->appendMessage(displayFrom, content, time, msgId);
                if (isNew && m_chatStack->currentWidget() != m_publicChat)
                    incUnread("public");
            } else if (msgType == "private") {
                QString to = obj["to"].toString();
                QString partner = (from == m_username) ? to : from;
                if (partner == "ALL" || partner.isEmpty()) continue;
                ChatWidget *chat = getOrCreatePrivateChat(partner);
                QString displayFrom = (from == m_username) ? QStringLiteral("我") : from;
                chat->appendMessage(displayFrom, content, time, msgId);
                if (isNew && m_chatStack->currentWidget() != chat)
                    incUnread(partner);
            } else if (msgType == "file") {
                int sep = content.indexOf("||");
                if (sep > 0) {
                    QString fname = content.left(sep);
                    QString b64 = content.mid(sep + 2);
                    ChatWidget *chat;
                    QString to = obj["to"].toString();
                    chat = (to == "ALL") ? m_publicChat
                                         : getOrCreatePrivateChat(to == m_username ? from : to);
                    handleReceivedFile(from, fname, 0, b64, time, chat);
                }
            } else if (msgType == "group") {
                int groupId = obj["to"].toString().toInt();
                ChatWidget *chat = m_groupChats.value(groupId);
                if (chat) {
                    QString displayFrom = (from == m_username) ? QStringLiteral("我") : from;
                    chat->appendMessage(displayFrom, content, time, msgId);
                }
            }
        }
        if (!messages.isEmpty()) {
            m_publicChat->appendSystemMessage(
                QStringLiteral("已加载 %1 条离线消息").arg(messages.size()));
            rebuildContactList();  // 刷新未读数字显示
        }
    }
    // 好友系统
    else if (type == MSG_FRIEND_REQUEST_RES) {
        onFriendRequestSent(data["ok"].toBool(), data["message"].toString());
    }
    else if (type == MSG_FRIEND_INCOMING) {
        onFriendRequestIncoming(data["from"].toString());
    }
    else if (type == MSG_FRIEND_ACCEPT_RES) {}
    else if (type == MSG_FRIEND_ACCEPT_NTF) {
        onFriendAccepted(data["from"].toString());
    }
    else if (type == MSG_FRIEND_LIST_RES) {
        m_friends.clear();
        QJsonArray friends = data["friends"].toArray();
        for (const auto &f : friends) {
            m_friends.append(f.toObject()["username"].toString());
        }
        rebuildGroupList();
        rebuildContactList();
    }
    else if (type == MSG_FRIEND_REMOVE_RES) {}
    else if (type == MSG_FRIEND_PENDING_LIST_RES) {
        m_pendingIncoming.clear();
        m_pendingOutgoing.clear();
        QJsonArray incoming = data["incoming"].toArray();
        QJsonArray outgoing = data["outgoing"].toArray();
        for (const auto &u : incoming)
            m_pendingIncoming.append(u.toObject()["username"].toString());
        for (const auto &u : outgoing)
            m_pendingOutgoing.append(u.toObject()["username"].toString());
    }
    // 群聊
    else if (type == MSG_GROUP_CREATE_RES) {
        bool ok = data["ok"].toBool();
        if (ok) {
            int groupId = data["group_id"].toInt();
            QString name = data["name"].toString();
            getOrCreateGroupChat(groupId, name);
            openGroupChat(groupId);
        }
    }
    else if (type == MSG_GROUP_INVITE_NTF) {
        int groupId = data["group_id"].toInt();
        QString groupName = data["group_name"].toString();
        QString from = data["from"].toString();
        ChatWidget *chat = getOrCreateGroupChat(groupId, groupName);
        chat->appendSystemMessage(QStringLiteral("你被 %1 邀请加入了群聊").arg(from));
        rebuildGroupList();
        rebuildContactList();
    }
    else if (type == MSG_GROUP_MSG) {
        int groupId = data["group_id"].toInt();
        QString from = data["from"].toString();
        QString content = data["content"].toString();
        QString time = data["time"].toString();
        int msgId = data["msg_id"].toInt();

        if (msgId > 0 && msgId > m_lastMsgId)
            m_lastMsgId = msgId;

        if (from == m_username) {
            ChatWidget *chat = m_groupChats.value(groupId);
            if (chat) chat->updateLastMsgId(msgId);
            return;
        }

        ChatWidget *chat = m_groupChats.value(groupId);
        if (!chat) {
            m_net->sendGroupList();
            return;
        }
        QString displayFrom = (from == m_username) ? QStringLiteral("我") : from;
        chat->appendMessage(displayFrom, content, time, msgId);

        if (m_chatStack->currentWidget() != chat)
            incUnread(QString("group:%1").arg(groupId), true);
    }
    else if (type == MSG_GROUP_LIST_RES) {
        QJsonArray groups = data["groups"].toArray();
        for (const auto &g : groups) {
            QJsonObject obj = g.toObject();
            int id = obj["id"].toInt();
            QString name = obj["name"].toString();
            if (!m_groupChats.contains(id))
                getOrCreateGroupChat(id, name);
        }
        rebuildGroupList();
        rebuildContactList();
    }
    else if (type == MSG_GROUP_LEAVE_RES) {
        bool ok = data["ok"].toBool();
        if (ok) {
            int groupId = data["group_id"].toInt();
            if (ChatWidget *chat = m_groupChats.value(groupId)) {
                if (m_chatStack->currentWidget() == chat)
                    m_chatStack->setCurrentWidget(m_publicChat);
                m_chatStack->removeWidget(chat);
                delete chat;
            }
            m_groupChats.remove(groupId);
            m_groupNames.remove(groupId);
            m_unreadGroup.remove(groupId);
            rebuildGroupList();
            rebuildContactList();
        }
    }
    else if (type == MSG_GROUP_MEMBERS_RES) {
        int groupId = data["group_id"].toInt();
        QString name = m_groupNames.value(groupId, QStringLiteral("群聊"));
        GroupMembersDialog::show(name, data["members"].toArray(), this);
    }
    else if (type == MSG_GROUP_INVITE_RES) {
        bool ok = data["ok"].toBool();
        QString msg = data["message"].toString(
            ok ? QStringLiteral("邀请已发送") : QString());
        QMessageBox::information(this, QStringLiteral("邀请好友"),
            ok ? QStringLiteral("邀请已发送") : msg);
    }
    else if (type == MSG_DELETE_ACCOUNT_RES) {
        bool ok = data["ok"].toBool();
        if (ok) {
            QMessageBox::information(this, QStringLiteral("注销"), QStringLiteral("账号已注销"));
            m_net->disconnect();
            close();
        } else {
            QMessageBox::warning(this, QStringLiteral("注销"), QStringLiteral("注销失败"));
        }
    }
}

void MainWindow::rebuildGroupList()
{
    m_groupList->clear();

    for (auto it = m_groupChats.begin(); it != m_groupChats.end(); ++it) {
        int gid = it.key();
        QString display = QStringLiteral("👥 %1").arg(m_groupNames.value(gid));
        int unread = m_unreadGroup.value(gid, 0);
        if (unread > 0)
            display += QStringLiteral("  [%1]").arg(unread);
        auto *item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, QString("group:%1").arg(gid));
        item->setForeground(QColor("#E0E0E0"));
        m_groupList->addItem(item);
    }
}

void MainWindow::rebuildContactList()
{
    m_contactList->clear();

    {
        QString display = QStringLiteral("💬 公共聊天");
        if (m_unreadPublic > 0)
            display += QStringLiteral("  [%1]").arg(m_unreadPublic);
        auto *item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, "public");
        item->setForeground(QColor("#E0E0E0"));
        m_contactList->addItem(item);
    }

    if (!m_friends.isEmpty()) {
        auto *friendHeader = new QListWidgetItem(QStringLiteral("─ 好友 ─"));
        friendHeader->setData(Qt::UserRole, "header");
        friendHeader->setFlags(friendHeader->flags() & ~Qt::ItemIsSelectable);
        friendHeader->setForeground(QColor("#888888"));
        m_contactList->addItem(friendHeader);

        for (const auto &f : m_friends) {
            QString display = QStringLiteral("● %1").arg(f);
            int unread = m_unreadPrivate.value(f, 0);
            if (unread > 0)
                display += QStringLiteral("  [%1]").arg(unread);
            auto *item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, f);
            m_contactList->addItem(item);
        }
    }

    if (!m_onlineUsers.isEmpty()) {
        QStringList nonFriendOnline;
        for (const auto &u : m_onlineUsers) {
            if (u != m_username && !m_friends.contains(u))
                nonFriendOnline.append(u);
        }
        if (!nonFriendOnline.isEmpty()) {
            auto *onlineHeader = new QListWidgetItem(QStringLiteral("─ 在线用户 ─"));
            onlineHeader->setData(Qt::UserRole, "online_header");
            onlineHeader->setFlags(onlineHeader->flags() & ~Qt::ItemIsSelectable);
            onlineHeader->setForeground(QColor("#888888"));
            m_contactList->addItem(onlineHeader);

            for (const auto &name : nonFriendOnline) {
                auto *item = new QListWidgetItem(name);
                item->setData(Qt::UserRole, name);
                item->setForeground(QColor("#AAAAAA"));
                m_contactList->addItem(item);
            }
        }
    }

    // 有未读消息但不是好友的用户（离线非好友）
    QStringList unreadKeys;
    for (auto it = m_unreadPrivate.begin(); it != m_unreadPrivate.end(); ++it) {
        if (it.value() > 0 && !m_friends.contains(it.key()) && !m_onlineUsers.contains(it.key()))
            unreadKeys.append(it.key());
    }
    if (!unreadKeys.isEmpty()) {
        auto *unreadHeader = new QListWidgetItem(QStringLiteral("─ 未读消息 ─"));
        unreadHeader->setData(Qt::UserRole, "unread_header");
        unreadHeader->setFlags(unreadHeader->flags() & ~Qt::ItemIsSelectable);
        unreadHeader->setForeground(QColor("#E67E22"));
        m_contactList->addItem(unreadHeader);

        for (const auto &name : unreadKeys) {
            QString display = QStringLiteral("○ %1  [%2]").arg(name).arg(m_unreadPrivate.value(name));
            auto *item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, name);
            item->setForeground(QColor("#E67E22"));
            m_contactList->addItem(item);
        }
    }
}

void MainWindow::updateUserList(const QJsonArray &users)
{
    m_onlineUsers.clear();
    for (const auto &u : users)
        m_onlineUsers.append(u.toString());

    rebuildGroupList();
    rebuildContactList();
}

void MainWindow::appendPublicMessage(const QString &from, const QString &content,
                                      const QString &time, int msgId)
{
    QString displayFrom = (from == m_username) ? QStringLiteral("我") : from;
    m_publicChat->appendMessage(displayFrom, content, time, msgId);
}

void MainWindow::appendPrivateMessage(const QString &from, const QString &content,
                                       const QString &time, const QString &target,
                                       int msgId)
{
    QString partner = (from == m_username) ? target : from;
    if (partner == "ALL" || partner.isEmpty()) return;

    ChatWidget *chat = getOrCreatePrivateChat(partner);
    QString displayFrom = (from == m_username) ? QStringLiteral("我") : from;
    chat->appendMessage(displayFrom, content, time, msgId);

    if (m_chatStack->currentWidget() != chat)
        incUnread(partner);
}

ChatWidget* MainWindow::getOrCreatePrivateChat(const QString &user)
{
    if (m_privateChats.contains(user))
        return m_privateChats[user];

    auto *chat = new ChatWidget(user, this);
    m_chatStack->addWidget(chat);
    m_privateChats[user] = chat;
    connect(chat, &ChatWidget::bubbleRightClicked,
            this, &MainWindow::showRecallMenu);
    connect(chat, &ChatWidget::imageClicked,
            this, [this](const QString &fp) { ImagePreviewDialog::show(fp, this); });
    return chat;
}

void MainWindow::openPrivateChat(const QString &user)
{
    ChatWidget *chat = getOrCreatePrivateChat(user);
    m_chatStack->setCurrentWidget(chat);
    m_chatHeader->setText(QStringLiteral("与 %1 聊天中").arg(user));
    clearUnread(user);
}

void MainWindow::onSendClicked()
{
    QString content = m_inputEdit->toPlainText().trimmed();
    if (content.isEmpty()) return;

    QWidget *current = m_chatStack->currentWidget();
    if (current == m_publicChat) {
        m_net->sendPublicMsg(content);
        QString now = QDateTime::currentDateTime().toString("hh:mm");
        m_publicChat->appendMessage(QStringLiteral("我"), content, now);
    } else {
        bool sent = false;
        for (auto it = m_privateChats.begin(); it != m_privateChats.end(); ++it) {
            if (it.value() == current) {
                m_net->sendPrivateMsg(it.key(), content);
                QString now = QDateTime::currentDateTime().toString("hh:mm");
                it.value()->appendMessage(QStringLiteral("我"), content, now);
                sent = true;
                break;
            }
        }
        if (!sent) {
            for (auto it = m_groupChats.begin(); it != m_groupChats.end(); ++it) {
                if (it.value() == current) {
                    m_net->sendGroupMsg(it.key(), content);
                    QString now = QDateTime::currentDateTime().toString("hh:mm");
                    it.value()->appendMessage(QStringLiteral("我"), content, now);
                    break;
                }
            }
        }
    }

    m_inputEdit->clear();
    m_inputEdit->setFocus();
}

void MainWindow::showFileDialog()
{
    QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("选择要发送的文件"));
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    QString filename = fi.fileName();
    qint64 filesize = fi.size();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法读取文件"));
        return;
    }
    QByteArray fileData = file.readAll();
    file.close();
    QString base64Data = QString::fromLatin1(fileData.toBase64());

    // 根据当前聊天确定发送目标
    QWidget *current = m_chatStack->currentWidget();
    QString target;
    bool isGroup = false;
    int groupId = 0;

    if (current == m_publicChat) {
        target = "ALL";
    } else {
        for (auto it = m_privateChats.begin(); it != m_privateChats.end(); ++it) {
            if (it.value() == current) {
                target = it.key();
                break;
            }
        }
        if (target.isEmpty()) {
            for (auto it = m_groupChats.begin(); it != m_groupChats.end(); ++it) {
                if (it.value() == current) {
                    groupId = it.key();
                    isGroup = true;
                    break;
                }
            }
            if (!isGroup) {
                QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个聊天"));
                return;
            }
        }
    }

    // 发送
    if (isGroup) {
        m_net->sendGroupFileMsg(groupId, filename, filesize, base64Data);
    } else {
        m_net->sendFileMsg(target, filename, filesize, base64Data);
    }

    // 本地显示
    QString now = QDateTime::currentDateTime().toString("hh:mm");
    ChatWidget *chat = isGroup ? m_groupChats.value(groupId)
                               : (target == "ALL" ? m_publicChat : getOrCreatePrivateChat(target));
    if (isImageFile(filename)) {
        chat->appendImageMessage(QStringLiteral("我"), filePath, filename, now);
    } else {
        chat->appendFileMessage(QStringLiteral("我"), filename, filesize, now);
    }
}

void MainWindow::handleReceivedFile(const QString &from, const QString &filename,
                                     qint64 filesize, const QString &base64Data,
                                     const QString &time, ChatWidget *chat)
{
    QDir().mkpath("received_files");
    QString savePath = QStringLiteral("received_files/%1_%2")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"), filename);

    QByteArray rawData = QByteArray::fromBase64(base64Data.toLatin1());
    QFile file(savePath);
    file.open(QIODevice::WriteOnly);
    file.write(rawData);
    file.close();

    QString displayFrom = (from == m_username) ? QStringLiteral("我") : from;

    if (isImageFile(filename)) {
        chat->appendImageMessage(displayFrom, savePath, filename, time);
    } else {
        chat->appendFileMessage(displayFrom, filename, filesize, time);
    }
}

bool MainWindow::isImageFile(const QString &filename)
{
    QString ext = QFileInfo(filename).suffix().toLower();
    return ext == "png" || ext == "jpg" || ext == "jpeg" ||
           ext == "gif" || ext == "bmp" || ext == "webp";
}

void MainWindow::handleRecallNtf(const QJsonObject &data)
{
    int msgId = data["msg_id"].toInt();
    QString from = data["from"].toString();
    QString target = data["target"].toString();
    QString msgType = data["msg_type"].toString();

    if (msgType == "public") {
        if (m_publicChat->removeMessage(msgId))
            m_publicChat->appendSystemMessage(
                QStringLiteral("%1 撤回了一条消息").arg(from == m_username ? "我" : from));
    } else if (msgType == "private") {
        QString partner = (from == m_username) ? target : from;
        ChatWidget *chat = m_privateChats.value(partner);
        if (chat && chat->removeMessage(msgId))
            chat->appendSystemMessage(
                QStringLiteral("%1 撤回了一条消息").arg(from == m_username ? "我" : from));
    } else if (msgType == "group") {
        int groupId = target.toInt();
        ChatWidget *chat = m_groupChats.value(groupId);
        if (chat && chat->removeMessage(msgId))
            chat->appendSystemMessage(
                QStringLiteral("%1 撤回了一条消息").arg(from == m_username ? "我" : from));
    }
}

void MainWindow::showRecallMenu(BubbleWidget *bubble)
{
    if (!bubble || !bubble->isSelf()) return;
    QMenu menu;
    menu.addAction(QStringLiteral("撤回"), this, [this, bubble]() {
        m_net->sendRecall(bubble->msgId());
    });
    menu.exec(QCursor::pos());
}

void MainWindow::requestOfflineMessages()
{
    m_net->sendOfflineQuery(0);  // 加载全部消息用于显示
}

void MainWindow::onAddFriend()
{
    QString target = QInputDialog::getText(this, QStringLiteral("添加好友"),
                                            QStringLiteral("请输入用户名："));
    if (target.isEmpty()) return;
    if (target == m_username) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("不能添加自己为好友"));
        return;
    }
    m_net->sendFriendRequest(target);
}

void MainWindow::onFriendManagement()
{
    m_net->sendFriendList();
    m_net->sendFriendPendingList();

    auto *dlg = new FriendManagementDialog(m_net, m_friends,
        m_pendingIncoming, m_pendingOutgoing, m_onlineUsers, m_username, this);

    dlg->setOnChanged([this]() {
        m_net->sendFriendList();
        m_net->sendFriendPendingList();
        rebuildGroupList();
        rebuildContactList();
    });

    dlg->exec();
    dlg->deleteLater();
}

void MainWindow::onFriendRequestIncoming(const QString &from)
{
    auto result = QMessageBox::question(this, QStringLiteral("好友请求"),
        QStringLiteral("%1 请求添加你为好友").arg(from),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes)
        m_net->sendFriendAccept(from);
    else
        m_net->sendFriendReject(from);
}

void MainWindow::onFriendRequestSent(bool ok, const QString &msg)
{
    QMessageBox::information(this, QStringLiteral("好友请求"),
                             ok ? QStringLiteral("好友请求已发送") : msg);
}

void MainWindow::onFriendAccepted(const QString &by)
{
    if (!m_friends.contains(by))
        m_friends.append(by);
    rebuildGroupList();
    rebuildContactList();

    ChatWidget *chat = getOrCreatePrivateChat(by);
    chat->appendSystemMessage(QStringLiteral("你们已经是好友了，开始聊天吧！"));
    m_publicChat->appendSystemMessage(QStringLiteral("你和 %1 已成为好友").arg(by));
}

void MainWindow::refreshFriendList()
{
    m_net->sendFriendList();
}

void MainWindow::onCreateGroup()
{
    m_net->sendFriendList();

    if (m_friends.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("好友列表为空，请先添加好友"));
        return;
    }

    auto *dlg = new CreateGroupDialog(m_friends, this);
    if (dlg->exec() == QDialog::Accepted) {
        auto result = dlg->result();
        m_net->sendGroupCreate(result.name, result.members);
    }
    delete dlg;
}

ChatWidget* MainWindow::getOrCreateGroupChat(int groupId, const QString &groupName)
{
    if (m_groupChats.contains(groupId))
        return m_groupChats[groupId];

    auto *chat = new ChatWidget(QString("group:%1").arg(groupId), this);
    m_chatStack->addWidget(chat);
    m_groupChats[groupId] = chat;
    m_groupNames[groupId] = groupName;
    chat->appendSystemMessage(QStringLiteral("欢迎加入群聊 %1").arg(groupName));
    connect(chat, &ChatWidget::bubbleRightClicked,
            this, &MainWindow::showRecallMenu);
    connect(chat, &ChatWidget::imageClicked,
            this, [this](const QString &fp) { ImagePreviewDialog::show(fp, this); });
    return chat;
}

void MainWindow::openGroupChat(int groupId)
{
    ChatWidget *chat = m_groupChats.value(groupId);
    if (!chat) return;
    m_chatStack->setCurrentWidget(chat);
    m_chatHeader->setText(m_groupNames.value(groupId, QStringLiteral("群聊")));
    clearUnread(QString("group:%1").arg(groupId), true);
}

void MainWindow::showGroupMembers(int groupId)
{
    m_net->sendGroupMembers(groupId);
}

void MainWindow::inviteToGroup(int groupId)
{
    if (m_friends.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("没有好友可邀请"));
        return;
    }

    bool ok;
    QString target = QInputDialog::getItem(this,
        QStringLiteral("邀请好友加入 %1").arg(m_groupNames.value(groupId)),
        QStringLiteral("选择要邀请的好友："),
        m_friends, 0, false, &ok);
    if (!ok || target.isEmpty()) return;

    m_net->sendGroupInvite(groupId, target);
}

void MainWindow::leaveGroup(int groupId)
{
    auto result = QMessageBox::question(this, QStringLiteral("退出群聊"),
        QStringLiteral("确定退出群聊「%1」吗？").arg(m_groupNames.value(groupId)),
        QMessageBox::Yes | QMessageBox::No);
    if (result != QMessageBox::Yes) return;

    m_net->sendGroupLeave(groupId);
}

void     MainWindow::incUnread(const QString &key, bool isGroup)
{
    if (isGroup) {
        int gid = key.mid(6).toInt();
        m_unreadGroup[gid] = m_unreadGroup.value(gid, 0) + 1;
        updateGroupBadge(gid, m_unreadGroup[gid]);
    } else if (key == "public") {
        m_unreadPublic++;
        updateContactBadge("public", m_unreadPublic);
    } else {
        m_unreadPrivate[key] = m_unreadPrivate.value(key, 0) + 1;
        updateContactBadge(key, m_unreadPrivate[key]);
    }
}

void MainWindow::clearUnread(const QString &key, bool isGroup)
{
    if (isGroup) {
        int gid = key.mid(6).toInt();
        m_unreadGroup[gid] = 0;
        updateGroupBadge(gid, 0);
    } else if (key == "public") {
        m_unreadPublic = 0;
        updateContactBadge("public", 0);
    } else {
        m_unreadPrivate[key] = 0;
        updateContactBadge(key, 0);
    }
}

void MainWindow::updateGroupBadge(int groupId, int count)
{
    QString key = QString("group:%1").arg(groupId);
    for (int i = 0; i < m_groupList->count(); i++) {
        QListWidgetItem *item = m_groupList->item(i);
        if (item->data(Qt::UserRole).toString() == key) {
            QString text = QStringLiteral("👥 %1").arg(m_groupNames.value(groupId, "群聊"));
            if (count > 0)
                text += QStringLiteral("  [%1]").arg(count);
            item->setText(text);
            break;
        }
    }
}

void MainWindow::updateContactBadge(const QString &key, int count)
{
    for (int i = 0; i < m_contactList->count(); i++) {
        QListWidgetItem *item = m_contactList->item(i);
        if (item->data(Qt::UserRole).toString() == key) {
            QString text = (key == "public") ? QStringLiteral("💬 公共聊天")
                                             : QStringLiteral("● %1").arg(key);
            if (count > 0)
                text += QStringLiteral("  [%1]").arg(count);
            item->setText(text);
            break;
        }
    }
}

void MainWindow::showHistoryDialog()
{
    HistoryDialog dlg(m_net, m_username, this);
    dlg.exec();
}

void MainWindow::onDisconnect()
{
    m_net->sendLogout();
    m_net->disconnect();
    emit logoutRequested();
    close();
}

void MainWindow::onDeleteAccount()
{
    auto result = QMessageBox::warning(this, QStringLiteral("注销账号"),
        QStringLiteral("确定要注销账号「%1」吗？此操作不可撤销，所有数据将被删除。").arg(m_username),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (result != QMessageBox::Yes) return;

    m_net->sendDeleteAccount();
}

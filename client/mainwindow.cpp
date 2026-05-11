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

// ============================================================
// BubbleWidget —— 聊天气泡
// ============================================================
BubbleWidget::BubbleWidget(const QString &content, const QString &time,
                           bool isSelf, int msgId, QWidget *parent)
    : QFrame(parent), m_msgId(msgId), m_isSelf(isSelf)
{
    QString bgColor = isSelf ? "#95EC69" : "#FFFFFF";
    int marginRight = isSelf ? 60 : 12;
    int marginLeft  = isSelf ? 12 : 60;

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(marginLeft, 3, marginRight, 3);
    layout->setSpacing(3);

    auto *timeLabel = new QLabel(time, this);
    timeLabel->setStyleSheet("color: #B0B0B0; font-size: 11px; padding: 0; background: transparent;");
    timeLabel->setAlignment(isSelf ? Qt::AlignRight : Qt::AlignLeft);

    m_bubbleLabel = new QLabel(content, this);
    m_bubbleLabel->setWordWrap(true);
    m_bubbleLabel->setMaximumWidth(420);
    m_bubbleLabel->setStyleSheet(QString(
        "background: %1; color: #353535; font-size: 14px;"
        "padding: 10px 14px; border-radius: 6px;"
    ).arg(bgColor));
    m_bubbleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    auto *shadow = new QGraphicsDropShadowEffect(m_bubbleLabel);
    shadow->setBlurRadius(8);
    shadow->setOffset(0, 1);
    shadow->setColor(QColor(0, 0, 0, 30));
    m_bubbleLabel->setGraphicsEffect(shadow);

    if (isSelf) {
        layout->addWidget(timeLabel);
        layout->addWidget(m_bubbleLabel, 0, Qt::AlignRight);
    } else {
        layout->addWidget(timeLabel);
        layout->addWidget(m_bubbleLabel, 0, Qt::AlignLeft);
    }

    setStyleSheet("background: transparent;");
}

void BubbleWidget::markRecalled()
{
    if (m_bubbleLabel)
        m_bubbleLabel->setStyleSheet(
            "background: #E8E8E8; color: #B0B0B0; font-size: 13px;"
            "padding: 10px 14px; border-radius: 6px;");
    m_bubbleLabel->setText(QStringLiteral("[消息已撤回]"));
}

// ============================================================
// ChatWidget —— 聊天区域（滚动气泡列表）
// ============================================================
ChatWidget::ChatWidget(const QString &chatWith, QWidget *parent)
    : QWidget(parent), m_chatWith(chatWith)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(
        "QScrollArea { background: #EDEDED; border: none; }");

    m_contentWidget = new QWidget(this);
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(12, 12, 12, 12);
    m_contentLayout->setSpacing(2);
    m_contentLayout->addStretch();

    m_scrollArea->setWidget(m_contentWidget);
    layout->addWidget(m_scrollArea);
}

bool ChatWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto *label = qobject_cast<QLabel*>(obj)) {
            QString fpath = label->property("imagePath").toString();
            if (!fpath.isEmpty()) {
                emit imageClicked(fpath);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ChatWidget::appendMessage(const QString &from, const QString &content,
                                const QString &time, int msgId)
{
    bool isSelf = (from == QStringLiteral("我"));

    if (!isSelf) {
        auto *nameLabel = new QLabel(from, m_contentWidget);
        nameLabel->setStyleSheet(
            "color: #888888; font-size: 12px; padding: 4px 60px 0 60px; background: transparent;");
        nameLabel->setAlignment(Qt::AlignLeft);
        m_contentLayout->insertWidget(m_contentLayout->count() - 1, nameLabel);
    }

    auto *bubble = new BubbleWidget(content, time, isSelf, msgId, m_contentWidget);
    m_contentLayout->insertWidget(m_contentLayout->count() - 1, bubble);

    if (msgId > 0)
        m_msgMap.insert(msgId, bubble);

    QTimer::singleShot(50, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum());
    });
}

void ChatWidget::appendSystemMessage(const QString &msg)
{
    auto *sysLabel = new QLabel(msg, m_contentWidget);
    sysLabel->setAlignment(Qt::AlignCenter);
    sysLabel->setStyleSheet(
        "color: #B0B0B0; font-size: 12px; padding: 8px; background: transparent;");
    sysLabel->setWordWrap(true);
    m_contentLayout->insertWidget(m_contentLayout->count() - 1, sysLabel);

    QTimer::singleShot(50, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum());
    });
}

void ChatWidget::appendImageMessage(const QString &from, const QString &filepath,
                                     const QString &filename, const QString &time)
{
    bool isSelf = (from == QStringLiteral("我"));

    if (!isSelf) {
        auto *nameLabel = new QLabel(from, m_contentWidget);
        nameLabel->setStyleSheet(
            "color: #888888; font-size: 12px; padding: 4px 60px 0 60px; background: transparent;");
        nameLabel->setAlignment(Qt::AlignLeft);
        m_contentLayout->insertWidget(m_contentLayout->count() - 1, nameLabel);
    }

    // 缩略图
    QPixmap pix(filepath);
    auto *imgLabel = new QLabel(m_contentWidget);
    if (!pix.isNull()) {
        QPixmap thumb = pix.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        imgLabel->setPixmap(thumb);
    }
    imgLabel->setCursor(Qt::PointingHandCursor);
    imgLabel->setToolTip(QStringLiteral("点击预览: %1").arg(filename));
    imgLabel->setProperty("imagePath", filepath);
    imgLabel->setStyleSheet("border-radius: 4px; padding: 4px; background: transparent;");
    imgLabel->installEventFilter(this);

    bool isLeft = !isSelf;
    int marginSide = isSelf ? 60 : 12;
    int marginOther = isSelf ? 12 : 60;

    auto *container = new QWidget(m_contentWidget);
    auto *clayout = new QVBoxLayout(container);
    clayout->setContentsMargins(marginOther, 2, marginSide, 2);
    clayout->setSpacing(2);

    auto *timeLabel = new QLabel(time, container);
    timeLabel->setStyleSheet("color: #B0B0B0; font-size: 11px; background: transparent;");
    timeLabel->setAlignment(isSelf ? Qt::AlignRight : Qt::AlignLeft);
    clayout->addWidget(timeLabel);
    clayout->addWidget(imgLabel, 0, isSelf ? Qt::AlignRight : Qt::AlignLeft);

    container->setStyleSheet("background: transparent;");
    m_contentLayout->insertWidget(m_contentLayout->count() - 1, container);

    QTimer::singleShot(50, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum());
    });
}

bool ChatWidget::removeMessage(int msgId)
{
    if (!m_msgMap.contains(msgId)) return false;
    BubbleWidget *bubble = m_msgMap.take(msgId);
    // 不直接删除，而是标记为已撤回
    bubble->markRecalled();
    return true;
}

// ============================================================
// MainWindow 实现
// ============================================================
MainWindow::MainWindow(ClientNetwork *net, const QString &username,
                       QWidget *parent)
    : QMainWindow(parent), m_net(net), m_username(username)
{
    setupUI();
    setWindowTitle(QStringLiteral("微信"));
    setMinimumSize(800, 560);
    resize(920, 620);

    connect(m_net, &ClientNetwork::messageReceived,
            this, &MainWindow::onMessageReceived);

    // 登录后刷新好友列表和离线消息
    QTimer::singleShot(500, this, [this]() {
        m_net->sendFriendList();
        m_net->sendGroupList();
        requestOfflineMessages();
    });
}

MainWindow::~MainWindow() {}

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
    // ── 菜单栏 ──
    auto *menuBar = new QMenuBar(this);

    auto *fileMenu = menuBar->addMenu(QStringLiteral("文件"));
    fileMenu->addAction(QStringLiteral("发送私有文件..."), this, [this](){ showFileDialog(true); });
    fileMenu->addAction(QStringLiteral("发送公共文件..."), this, [this](){ showFileDialog(false); });
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("添加好友..."), this, &MainWindow::onAddFriend);
    fileMenu->addAction(QStringLiteral("好友管理..."), this, &MainWindow::onFriendManagement);
    fileMenu->addAction(QStringLiteral("创建群聊..."), this, &MainWindow::onCreateGroup);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("断开连接"), this, &MainWindow::onDisconnect);

    auto *viewMenu = menuBar->addMenu(QStringLiteral("查看"));
    viewMenu->addAction(QStringLiteral("聊天历史..."), this, [this](){ showHistoryDialog(); });
    setMenuBar(menuBar);

    // ── 中央控件 ──
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ======== 左侧面板 ========
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

    // 搜索栏
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

    // ── 群组列表 ──
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

    // ── 分隔线 ──
    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #3A3F45; margin: 2px 10px;");
    leftLayout->addWidget(sep);

    // ── 联系人列表 ──
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

    // ======== 右侧面板 ========
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

    rightLayout->addWidget(m_chatStack, 1);

    // 底部输入区域
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

    auto *attachBtn = new QPushButton(QStringLiteral("📎"), this);
    attachBtn->setFixedSize(30, 30);
    attachBtn->setCursor(Qt::PointingHandCursor);
    attachBtn->setToolTip(QStringLiteral("发送文件"));
    attachBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; font-size: 16px; }"
        "QPushButton:hover { background: #E8E8E8; border-radius: 4px; }");

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

    // ── 连接信号 ──
    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(attachBtn, &QPushButton::clicked, this, [this](){ showFileDialog(true); });

    // 群组列表：双击打开群聊
    connect(m_groupList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *item) {
        QString role = item->data(Qt::UserRole).toString();
        if (role.startsWith("group:"))
            openGroupChat(role.mid(6).toInt());
    });

    // 群组列表：右键菜单
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

    // 联系人列表：双击打开私聊或公共聊天
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

    // 联系人列表：右键菜单
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

// ============================================================
// 消息处理
// ============================================================
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

        // 自己发的消息已在 onSendClicked 本地添加，跳过服务器回显
        if (from == m_username)
            return;

        if (msgType == "public") {
            appendPublicMessage(from, content, time, msgId);
            // 未读
            if (m_chatStack->currentWidget() != m_publicChat)
                incUnread("public");
        } else {
            appendPrivateMessage(from, content, time, to, msgId);
        }
    }
    else if (type == MSG_FILE_INCOMING) {
        int fileId = data["file_id"].toInt();
        QString from = data["from"].toString();
        QString filename = data["filename"].toString();
        qint64 filesize = data["filesize"].toVariant().toLongLong();
        showIncomingFileDialog(fileId, from, filename, filesize);
    }
    else if (type == MSG_FILE_DATA_FWD) {
        int fileId = data["file_id"].toInt();
        if (m_downloads.contains(fileId)) {
            FileDownload &dl = m_downloads[fileId];
            QByteArray chunk = QByteArray::fromBase64(data["data"].toString().toUtf8());
            dl.file->write(chunk);
            dl.chunksRecv++;
        }
    }
    else if (type == MSG_FILE_END_FWD) {
        int fileId = data["file_id"].toInt();
        if (m_downloads.contains(fileId)) {
            FileDownload &dl = m_downloads[fileId];
            dl.file->close();
            delete dl.file;
            dl.file = nullptr;

            // 判断是否为图片
            if (isImageFile(dl.filename)) {
                // 在聊天中显示图片消息
                ChatWidget *chat = m_privateChats.value(dl.from);
                if (chat) {
                    QString now = QDateTime::currentDateTime().toString("hh:mm");
                    chat->appendImageMessage(QStringLiteral("对方"), dl.filepath, dl.filename, now);
                    connect(chat, &ChatWidget::imageClicked,
                            this, &MainWindow::showImagePreview);
                }
            }

            QMessageBox::information(this, QStringLiteral("收到文件"),
                QStringLiteral("收到来自 %2 的文件'%1'\n已保存至：%3")
                .arg(dl.filename, dl.from, dl.filepath));

            if (m_privateChats.contains(dl.from))
                m_privateChats[dl.from]->appendSystemMessage(
                    QStringLiteral("收到文件：%1").arg(dl.filename));
            m_downloads.remove(fileId);
        }
    }
    else if (type == MSG_HISTORY_RES) {
        showHistoryResults(data);
    }
    else if (type == MSG_ERROR) {
        QMessageBox::warning(this, QStringLiteral("错误"), data["message"].toString());
    }
    // ---- 消息撤回 ----
    else if (type == MSG_RECALL_RES) {
        bool ok = data["ok"].toBool();
        if (!ok) QMessageBox::information(this, QStringLiteral("撤回"), data["message"].toString());
    }
    else if (type == MSG_RECALL_NTF) {
        handleRecallNtf(data);
    }
    // ---- 离线消息 ----
    else if (type == MSG_OFFLINE_RES) {
        QJsonArray messages = data["messages"].toArray();
        for (const auto &m : messages) {
            QJsonObject obj = m.toObject();
            int msgId = obj["msg_id"].toInt();
            if (msgId > m_lastMsgId) m_lastMsgId = msgId;

            QString from = obj["from"].toString();
            QString content = obj["content"].toString();
            QString time = obj["time"].toString();
            QString msgType = obj["msg_type"].toString();
            int recalled = obj["recalled"].toInt();

            if (recalled) continue; // 已撤回的跳过

            if (msgType == "public") {
                appendPublicMessage(from, content, time, msgId);
            } else if (msgType == "private") {
                QString to = obj["to"].toString();
                appendPrivateMessage(from, content, time, to, msgId);
            } else if (msgType == "group") {
                int groupId = obj["to"].toString().toInt();
                ChatWidget *chat = m_groupChats.value(groupId);
                if (chat) {
                    QString displayFrom = (from == m_username) ? QStringLiteral("我") : from;
                    chat->appendMessage(displayFrom, content, time, msgId);
                }
            }
        }
        if (!messages.isEmpty())
            m_publicChat->appendSystemMessage(
                QStringLiteral("已加载 %1 条离线消息").arg(messages.size()));
    }
    // ---- 好友系统 ----
    else if (type == MSG_FRIEND_REQUEST_RES) {
        onFriendRequestSent(data["ok"].toBool(), data["message"].toString());
    }
    else if (type == MSG_FRIEND_INCOMING) {
        onFriendRequestIncoming(data["from"].toString());
    }
    else if (type == MSG_FRIEND_ACCEPT_RES) {
        // 接收方确认
    }
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
    else if (type == MSG_FRIEND_REMOVE_RES) {
        // 删除完成
    }
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
    // ---- 群聊 ----
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

        // 自己发的群消息已在 onSendClicked 本地添加，跳过服务器回显
        if (from == m_username)
            return;

        ChatWidget *chat = m_groupChats.value(groupId);
        if (!chat) {
            // 未知群组，请求列表
            m_net->sendGroupList();
            return;
        }
        QString displayFrom = (from == m_username) ? QStringLiteral("我") : from;
        chat->appendMessage(displayFrom, content, time, msgId);

        // 未读
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
        onGroupMembers(data);
    }
    else if (type == MSG_GROUP_INVITE_RES) {
        onGroupInviteRes(data);
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

    // 公共聊天
    {
        QString display = QStringLiteral("💬 公共聊天");
        if (m_unreadPublic > 0)
            display += QStringLiteral("  [%1]").arg(m_unreadPublic);
        auto *item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, "public");
        item->setForeground(QColor("#E0E0E0"));
        m_contactList->addItem(item);
    }

    // ── 好友区域 ──
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

    // ── 在线用户区域（非好友） ──
    if (!m_onlineUsers.isEmpty()) {
        // 过滤掉已是好友的用户
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
}

// 更新用户列表（被 MSG_USER_LIST_RES 调用）
void MainWindow::updateUserList(const QJsonArray &users)
{
    // 保存在线用户列表
    m_onlineUsers.clear();
    for (const auto &u : users)
        m_onlineUsers.append(u.toString());

    rebuildGroupList();
    rebuildContactList();
}

// ============================================================
// 聊天消息追加
// ============================================================
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

    // 未读计数
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
        // 私聊
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
        // 群聊
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

// ============================================================
// 文件传输
// ============================================================
void MainWindow::onSendFile() { showFileDialog(true); }

void MainWindow::showFileDialog(bool isPrivate)
{
    QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("选择要发送的文件"));
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    QString filename = fi.fileName();
    qint64 filesize = fi.size();

    QString target;
    if (isPrivate) {
        target = QInputDialog::getText(this, QStringLiteral("发送文件"),
                                        QStringLiteral("请输入接收者用户名："));
        if (target.isEmpty()) return;
    } else {
        target = "ALL";
    }

    QString fileType = isPrivate ? "private" : "public";
    m_net->sendFileMeta(target, filename, filesize, fileType);

    QMessageBox::information(this, QStringLiteral("文件传输"),
        QStringLiteral("正在发送文件'%1'（%2 字节）...").arg(filename).arg(filesize));
}

void MainWindow::showIncomingFileDialog(int fileId, const QString &from,
                                         const QString &filename, qint64 filesize)
{
    QString msg = QStringLiteral("来自 %1 的文件：\n%2（%3 字节）\n是否接受？")
        .arg(from, filename).arg(filesize);

    auto result = QMessageBox::question(this, QStringLiteral("文件传输"), msg,
                                         QMessageBox::Yes | QMessageBox::No);
    if (result == QMessageBox::Yes) {
        QString savePath = QFileDialog::getSaveFileName(this, QStringLiteral("保存文件"), filename);
        if (savePath.isEmpty()) return;

        FileDownload dl;
        dl.from = from;
        dl.filename = filename;
        dl.filepath = savePath;
        dl.file = new QFile(savePath);
        dl.file->open(QIODevice::WriteOnly);
        m_downloads.insert(fileId, dl);

        m_net->sendMessage(QJsonObject{
            {"type", MSG_FILE_ACCEPT},
            {"data", QJsonObject{{"file_id", fileId}, {"target", from}}}
        });

        openPrivateChat(from);
        m_privateChats[from]->appendSystemMessage(
            QStringLiteral("正在接收文件：%1...").arg(filename));
    }
}

// ============================================================
// 图片预览
// ============================================================
bool MainWindow::isImageFile(const QString &filename)
{
    QString ext = QFileInfo(filename).suffix().toLower();
    return ext == "png" || ext == "jpg" || ext == "jpeg" ||
           ext == "gif" || ext == "bmp" || ext == "webp";
}

void MainWindow::showImagePreview(const QString &filepath)
{
    QPixmap pix(filepath);
    if (pix.isNull()) {
        QMessageBox::warning(this, QStringLiteral("预览失败"),
                             QStringLiteral("无法加载图片"));
        return;
    }

    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("图片预览"));
    dlg->resize(600, 500);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto *layout = new QVBoxLayout(dlg);
    auto *label = new QLabel(dlg);
    label->setPixmap(pix.scaled(560, 440, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("background: #1E1E1E; border-radius: 4px; padding: 10px;");
    layout->addWidget(label);

    auto *btnLayout = new QHBoxLayout;
    auto *openBtn = new QPushButton(QStringLiteral("打开原图"), dlg);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), dlg);
    btnLayout->addStretch();
    btnLayout->addWidget(openBtn);
    btnLayout->addWidget(closeBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(openBtn, &QPushButton::clicked, [filepath]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filepath));
    });
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);

    dlg->show();
}

// ============================================================
// 消息撤回
// ============================================================
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

// ============================================================
// 离线消息
// ============================================================
void MainWindow::requestOfflineMessages()
{
    m_net->sendOfflineQuery(m_lastMsgId);
}

// ============================================================
// 好友系统
// ============================================================
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
    // 刷新好友列表和待处理请求
    m_net->sendFriendList();
    m_net->sendFriendPendingList();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("好友管理"));
    dlg.resize(360, 450);

    auto *layout = new QVBoxLayout(&dlg);

    auto *listWidget = new QListWidget(&dlg);
    listWidget->setStyleSheet("QListWidget::item { padding: 8px; font-size: 14px; }");
    layout->addWidget(listWidget);

    auto *btnLayout = new QHBoxLayout;
    auto *addBtn = new QPushButton(QStringLiteral("添加好友"), &dlg);
    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), &dlg);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), &dlg);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(refreshBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);

    // --- 弹出右键菜单（含接受/拒绝/删除）---
    listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(listWidget, &QListWidget::customContextMenuRequested,
            this, [this, listWidget](const QPoint &pos) {
        QListWidgetItem *item = listWidget->itemAt(pos);
        if (!item) return;
        QString tag = item->data(Qt::UserRole).toString();
        QString raw = item->data(Qt::UserRole + 1).toString(); // category
        if (tag.isEmpty()) return;

        QMenu menu;
        if (raw == "incoming") {
            // 收到的待处理请求 → 可接受或拒绝
            menu.addAction(QStringLiteral("接受好友"), this, [this, tag]() {
                m_net->sendFriendAccept(tag);
                m_pendingIncoming.removeAll(tag);
                QTimer::singleShot(500, this, [this]() { m_net->sendFriendList(); });
            });
            menu.addAction(QStringLiteral("拒绝"), this, [this, tag]() {
                m_net->sendFriendReject(tag);
                m_pendingIncoming.removeAll(tag);
            });
        } else if (raw == "friend") {
            // 已是好友 → 可删除
            menu.addAction(QStringLiteral("删除好友"), this, [this, tag]() {
                auto result = QMessageBox::question(this, QStringLiteral("删除好友"),
                    QStringLiteral("确定删除好友 %1 吗？").arg(tag));
                if (result == QMessageBox::Yes) {
                    m_net->sendFriendRemove(tag);
                    m_friends.removeAll(tag);
                    rebuildGroupList();
                    rebuildContactList();
                }
            });
        }
        menu.exec(listWidget->mapToGlobal(pos));
    });

    // --- 填充列表 ---
    auto populateList = [this, listWidget]() {
        listWidget->clear();

        // 收到的待处理请求
        if (!m_pendingIncoming.isEmpty()) {
            auto *secHeader = new QListWidgetItem(QStringLiteral("─ 待接受的好友请求 ─"));
            secHeader->setFlags(secHeader->flags() & ~Qt::ItemIsSelectable);
            secHeader->setForeground(QColor("#E67E22"));
            listWidget->addItem(secHeader);

            for (const auto &u : m_pendingIncoming) {
                QString display = QStringLiteral("📩 %1  [右键接受/拒绝]").arg(u);
                auto *item = new QListWidgetItem(display);
                item->setData(Qt::UserRole, u);
                item->setData(Qt::UserRole + 1, "incoming");
                item->setForeground(QColor("#E67E22"));
                listWidget->addItem(item);
            }
        }

        // 发出的待处理请求
        if (!m_pendingOutgoing.isEmpty()) {
            auto *secHeader = new QListWidgetItem(QStringLiteral("─ 等待对方接受 ─"));
            secHeader->setFlags(secHeader->flags() & ~Qt::ItemIsSelectable);
            secHeader->setForeground(QColor("#3498DB"));
            listWidget->addItem(secHeader);

            for (const auto &u : m_pendingOutgoing) {
                QString display = QStringLiteral("⏳ %1  [等待中]").arg(u);
                auto *item = new QListWidgetItem(display);
                item->setForeground(QColor("#888888"));
                listWidget->addItem(item);
            }
        }

        // 当前好友
        if (!m_friends.isEmpty()) {
            auto *secHeader = new QListWidgetItem(QStringLiteral("─ 好友列表 ─"));
            secHeader->setFlags(secHeader->flags() & ~Qt::ItemIsSelectable);
            secHeader->setForeground(QColor("#888888"));
            listWidget->addItem(secHeader);

            for (const auto &f : m_friends) {
                bool online = m_onlineUsers.contains(f);
                QString display = online ? QStringLiteral("● %1  [在线]").arg(f)
                                         : QStringLiteral("○ %1  [离线]").arg(f);
                auto *item = new QListWidgetItem(display);
                item->setData(Qt::UserRole, f);
                item->setData(Qt::UserRole + 1, "friend");
                item->setForeground(online ? QColor("#07C160") : QColor("#AAAAAA"));
                listWidget->addItem(item);
            }
        }

        if (m_pendingIncoming.isEmpty() && m_pendingOutgoing.isEmpty() && m_friends.isEmpty()) {
            listWidget->addItem(QStringLiteral("（暂无好友和申请）"));
        }
    };

    // 初始填充 + 定时刷新
    populateList();

    connect(refreshBtn, &QPushButton::clicked, this, [this, populateList]() {
        m_net->sendFriendList();
        m_net->sendFriendPendingList();
        QTimer::singleShot(500, populateList);
    });

    connect(addBtn, &QPushButton::clicked, this, [this, populateList]() {
        onAddFriend();
        QTimer::singleShot(800, this, [this, populateList]() {
            m_net->sendFriendList();
            m_net->sendFriendPendingList();
            QTimer::singleShot(500, populateList);
        });
    });

    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::close);
    dlg.exec();
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

// ============================================================
// 群聊
// ============================================================
void MainWindow::onCreateGroup()
{
    // 先刷新好友列表
    m_net->sendFriendList();

    QString name = QInputDialog::getText(this, QStringLiteral("创建群聊"),
                                          QStringLiteral("群名称："));
    if (name.isEmpty()) return;

    // 如果没有好友，提示
    if (m_friends.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("好友列表为空，请先添加好友"));
        return;
    }

    // 成员选择对话框
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("选择群成员"));
    dlg.resize(280, 320);

    auto *layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel(QStringLiteral("请选择要加入群聊的好友："), &dlg));

    auto *listWidget = new QListWidget(&dlg);
    for (const auto &f : m_friends) {
        auto *item = new QListWidgetItem(f);
        item->setCheckState(Qt::Checked);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        listWidget->addItem(item);
    }
    listWidget->setStyleSheet("QListWidget::item { padding: 6px; }");
    layout->addWidget(listWidget);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    QStringList members;
    for (int i = 0; i < listWidget->count(); i++) {
        auto *item = listWidget->item(i);
        if (item->checkState() == Qt::Checked)
            members.append(item->text());
    }

    if (members.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("请至少选择一名成员"));
        return;
    }

    m_net->sendGroupCreate(name, members);
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

void MainWindow::onGroupMembers(const QJsonObject &data)
{
    int groupId = data["group_id"].toInt();
    QJsonArray members = data["members"].toArray();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("群成员 - %1").arg(m_groupNames.value(groupId)));
    dlg.resize(280, 320);

    auto *layout = new QVBoxLayout(&dlg);
    auto *label = new QLabel(QStringLiteral("群成员（%1 人）").arg(members.size()), &dlg);
    label->setStyleSheet("font-size: 14px; font-weight: bold; padding: 6px;");
    layout->addWidget(label);

    auto *listWidget = new QListWidget(&dlg);
    for (const auto &m : members) {
        QJsonObject obj = m.toObject();
        QString name = obj["username"].toString();
        bool online = obj["online"].toBool();
        QString display = online ? QStringLiteral("● %1").arg(name)
                                 : QStringLiteral("○ %1").arg(name);
        auto *item = new QListWidgetItem(display);
        item->setForeground(online ? QColor("#07C160") : QColor("#AAAAAA"));
        listWidget->addItem(item);
    }
    listWidget->setStyleSheet(
        "QListWidget { font-size: 13px; border: none; }"
        "QListWidget::item { padding: 6px 10px; }");
    layout->addWidget(listWidget);

    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::close);
    layout->addWidget(closeBtn, 0, Qt::AlignCenter);

    dlg.exec();
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

void MainWindow::onGroupInviteRes(const QJsonObject &data)
{
    bool ok = data["ok"].toBool();
    QString msg = data["message"].toString(
        ok ? QStringLiteral("邀请已发送") : QString());
    QMessageBox::information(this, QStringLiteral("邀请好友"),
        ok ? QStringLiteral("邀请已发送") : msg);
}

void MainWindow::leaveGroup(int groupId)
{
    auto result = QMessageBox::question(this, QStringLiteral("退出群聊"),
        QStringLiteral("确定退出群聊「%1」吗？").arg(m_groupNames.value(groupId)),
        QMessageBox::Yes | QMessageBox::No);
    if (result != QMessageBox::Yes) return;

    m_net->sendGroupLeave(groupId);
}

// ============================================================
// 未读消息
// ============================================================
void MainWindow::incUnread(const QString &key, bool isGroup)
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

// ============================================================
// 历史记录
// ============================================================
void MainWindow::onShowHistory() { showHistoryDialog(); }

void MainWindow::showHistoryDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("聊天与文件历史"));
    dlg.resize(520, 450);

    auto *layout = new QVBoxLayout(&dlg);
    auto *tabWidget = new QTabWidget(&dlg);
    layout->addWidget(tabWidget);

    // ── 聊天记录 ──
    auto *chatPage = new QWidget(&dlg);
    auto *chatLayout = new QVBoxLayout(chatPage);
    auto *chatTypeCombo = new QComboBox(chatPage);
    chatTypeCombo->addItems({QStringLiteral("public"), QStringLiteral("private"), QStringLiteral("all")});
    auto *chatTargetEdit = new QLineEdit(chatPage);
    chatTargetEdit->setPlaceholderText(QStringLiteral("用户名（查私聊记录）"));
    auto *chatDisplay = new QTextEdit(chatPage);
    chatDisplay->setReadOnly(true);
    auto *chatBtn = new QPushButton(QStringLiteral("搜索聊天记录"), chatPage);
    chatBtn->setStyleSheet(
        "QPushButton { background: #07C160; color: white; border: none;"
        "  border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background: #06AD56; }");
    chatLayout->addWidget(new QLabel(QStringLiteral("类型：")));
    chatLayout->addWidget(chatTypeCombo);
    chatLayout->addWidget(new QLabel(QStringLiteral("目标：")));
    chatLayout->addWidget(chatTargetEdit);
    chatLayout->addWidget(chatBtn);
    chatLayout->addWidget(chatDisplay);
    tabWidget->addTab(chatPage, QStringLiteral("聊天记录"));

    // ── 文件记录 ──
    auto *filePage = new QWidget(&dlg);
    auto *fileLayout = new QVBoxLayout(filePage);
    auto *fileTypeCombo = new QComboBox(filePage);
    fileTypeCombo->addItems({QStringLiteral("public"), QStringLiteral("private"), QStringLiteral("all")});
    auto *fileTargetEdit = new QLineEdit(filePage);
    fileTargetEdit->setPlaceholderText(QStringLiteral("用户名（查私有文件）"));
    auto *fileDisplay = new QTextEdit(filePage);
    fileDisplay->setReadOnly(true);
    auto *fileBtn = new QPushButton(QStringLiteral("搜索文件记录"), filePage);
    fileBtn->setStyleSheet(
        "QPushButton { background: #07C160; color: white; border: none;"
        "  border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background: #06AD56; }");
    fileLayout->addWidget(new QLabel(QStringLiteral("类型：")));
    fileLayout->addWidget(fileTypeCombo);
    fileLayout->addWidget(new QLabel(QStringLiteral("目标：")));
    fileLayout->addWidget(fileTargetEdit);
    fileLayout->addWidget(fileBtn);
    fileLayout->addWidget(fileDisplay);
    tabWidget->addTab(filePage, QStringLiteral("文件记录"));

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(btnBox);

    // 搜索聊天
    connect(chatBtn, &QPushButton::clicked, [this, chatTypeCombo, chatTargetEdit, chatDisplay]() {
        QString type = chatTypeCombo->currentText();
        QString target = chatTargetEdit->text().trimmed();
        if (target.isEmpty()) target = m_username;
        chatDisplay->clear();
        chatDisplay->append(QStringLiteral("搜索中..."));

        QMetaObject::Connection *conn = new QMetaObject::Connection;
        *conn = connect(m_net, &ClientNetwork::messageReceived,
                        [this, chatDisplay, conn](const QJsonObject &msg) {
            if (msg["type"].toString() == MSG_HISTORY_RES) {
                QJsonObject data = msg["data"].toObject();
                chatDisplay->clear();
                QJsonArray msgs = data["messages"].toArray();
                if (msgs.isEmpty())
                    chatDisplay->append(QStringLiteral("未找到消息。"));
                else
                    for (const auto &m : msgs) {
                        QJsonObject obj = m.toObject();
                        QString content = obj["content"].toString();
                        if (obj["recalled"].toInt() == 1)
                            content = QStringLiteral("[消息已撤回]");
                        chatDisplay->append(QStringLiteral("[%1] %2 → %3: %4")
                            .arg(obj["time"].toString(), obj["sender"].toString(),
                                 obj["target"].toString(), content));
                    }
                disconnect(*conn);
                delete conn;
            }
        });
        m_net->sendHistoryQuery(type, target, 200);
    });

    // 搜索文件
    connect(fileBtn, &QPushButton::clicked, [this, fileTypeCombo, fileTargetEdit, fileDisplay]() {
        QString type = fileTypeCombo->currentText();
        QString target = fileTargetEdit->text().trimmed();
        if (target.isEmpty()) target = m_username;
        fileDisplay->clear();
        fileDisplay->append(QStringLiteral("搜索中..."));

        QMetaObject::Connection *conn = new QMetaObject::Connection;
        *conn = connect(m_net, &ClientNetwork::messageReceived,
                        [this, fileDisplay, conn](const QJsonObject &msg) {
            if (msg["type"].toString() == MSG_HISTORY_RES) {
                QJsonObject data = msg["data"].toObject();
                fileDisplay->clear();
                QJsonArray files = data["files"].toArray();
                if (files.isEmpty())
                    fileDisplay->append(QStringLiteral("未找到文件。"));
                else
                    for (const auto &f : files) {
                        QJsonObject obj = f.toObject();
                        fileDisplay->append(QStringLiteral("[%1] %2 → %3: %4（%5 字节）")
                            .arg(obj["time"].toString(), obj["sender"].toString(),
                                 obj["target"].toString(), obj["filename"].toString(),
                                 QString::number(obj["filesize"].toInt())));
                    }
                disconnect(*conn);
                delete conn;
            }
        });
        m_net->sendHistoryQuery(type == "all" ? "file" : type, target, 200);
    });

    dlg.exec();
}

void MainWindow::onDisconnect()
{
    m_net->sendLogout();
    m_net->disconnect();
    close();
}

void MainWindow::showHistoryResults(const QJsonObject &data)
{
    Q_UNUSED(data);
}

#include "frienddlg.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>
#include <QJsonObject>

#include "../../client/clientnetwork.h"

FriendManagementDialog::FriendManagementDialog(ClientNetwork *net,
        const QStringList &friends, const QStringList &pendingIncoming,
        const QStringList &pendingOutgoing, const QStringList &onlineUsers,
        const QString &selfUsername, QWidget *parent)
    : QDialog(parent), m_net(net), m_friends(friends),
      m_pendingIncoming(pendingIncoming), m_pendingOutgoing(pendingOutgoing),
      m_onlineUsers(onlineUsers), m_selfUsername(selfUsername)
{
    setWindowTitle(QStringLiteral("好友管理"));
    resize(360, 450);

    auto *layout = new QVBoxLayout(this);

    m_listWidget = new QListWidget(this);
    m_listWidget->setStyleSheet("QListWidget::item { padding: 8px; font-size: 14px; }");
    layout->addWidget(m_listWidget);

    auto *btnLayout = new QHBoxLayout;
    auto *addBtn = new QPushButton(QStringLiteral("添加好友"), this);
    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(refreshBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);

    // 右键菜单
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listWidget, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        QListWidgetItem *item = m_listWidget->itemAt(pos);
        if (!item) return;
        QString tag = item->data(Qt::UserRole).toString();
        QString raw = item->data(Qt::UserRole + 1).toString();
        if (tag.isEmpty()) return;

        QMenu menu;
        if (raw == "incoming") {
            menu.addAction(QStringLiteral("接受好友"), this, [this, tag]() {
                m_net->sendFriendAccept(tag);
                m_pendingIncoming.removeAll(tag);
                if (m_onChanged) m_onChanged();
            });
            menu.addAction(QStringLiteral("拒绝"), this, [this, tag]() {
                m_net->sendFriendReject(tag);
                m_pendingIncoming.removeAll(tag);
                populateList();
            });
        } else if (raw == "friend") {
            menu.addAction(QStringLiteral("删除好友"), this, [this, tag]() {
                auto result = QMessageBox::question(this, QStringLiteral("删除好友"),
                    QStringLiteral("确定删除好友 %1 吗？").arg(tag));
                if (result == QMessageBox::Yes) {
                    m_net->sendFriendRemove(tag);
                    m_friends.removeAll(tag);
                    if (m_onChanged) m_onChanged();
                }
            });
        }
        menu.exec(m_listWidget->mapToGlobal(pos));
    });

    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        m_net->sendFriendList();
        m_net->sendFriendPendingList();
        if (m_onChanged) m_onChanged();
    });

    connect(addBtn, &QPushButton::clicked, this, [this]() {
        QString target = QInputDialog::getText(this, QStringLiteral("添加好友"),
                                                QStringLiteral("请输入用户名："));
        if (target.isEmpty()) return;
        if (target == m_selfUsername) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("不能添加自己为好友"));
            return;
        }
        m_net->sendFriendRequest(target);
        if (m_onChanged) m_onChanged();
    });

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

    populateList();
}

void FriendManagementDialog::populateList()
{
    m_listWidget->clear();

    if (!m_pendingIncoming.isEmpty()) {
        auto *secHeader = new QListWidgetItem(QStringLiteral("─ 待接受的好友请求 ─"));
        secHeader->setFlags(secHeader->flags() & ~Qt::ItemIsSelectable);
        secHeader->setForeground(QColor("#E67E22"));
        m_listWidget->addItem(secHeader);

        for (const auto &u : m_pendingIncoming) {
            QString display = QStringLiteral("📩 %1  [右键接受/拒绝]").arg(u);
            auto *item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, u);
            item->setData(Qt::UserRole + 1, "incoming");
            item->setForeground(QColor("#E67E22"));
            m_listWidget->addItem(item);
        }
    }

    if (!m_pendingOutgoing.isEmpty()) {
        auto *secHeader = new QListWidgetItem(QStringLiteral("─ 等待对方接受 ─"));
        secHeader->setFlags(secHeader->flags() & ~Qt::ItemIsSelectable);
        secHeader->setForeground(QColor("#3498DB"));
        m_listWidget->addItem(secHeader);

        for (const auto &u : m_pendingOutgoing) {
            QString display = QStringLiteral("⏳ %1  [等待中]").arg(u);
            auto *item = new QListWidgetItem(display);
            item->setForeground(QColor("#888888"));
            m_listWidget->addItem(item);
        }
    }

    if (!m_friends.isEmpty()) {
        auto *secHeader = new QListWidgetItem(QStringLiteral("─ 好友列表 ─"));
        secHeader->setFlags(secHeader->flags() & ~Qt::ItemIsSelectable);
        secHeader->setForeground(QColor("#888888"));
        m_listWidget->addItem(secHeader);

        for (const auto &f : m_friends) {
            bool online = m_onlineUsers.contains(f);
            QString display = online ? QStringLiteral("● %1  [在线]").arg(f)
                                     : QStringLiteral("○ %1  [离线]").arg(f);
            auto *item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, f);
            item->setData(Qt::UserRole + 1, "friend");
            item->setForeground(online ? QColor("#07C160") : QColor("#AAAAAA"));
            m_listWidget->addItem(item);
        }
    }

    if (m_pendingIncoming.isEmpty() && m_pendingOutgoing.isEmpty() && m_friends.isEmpty()) {
        m_listWidget->addItem(QStringLiteral("（暂无好友和申请）"));
    }
}

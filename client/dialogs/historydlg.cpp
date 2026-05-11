#include "historydlg.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QJsonObject>
#include <QJsonArray>

#include "../../client/clientnetwork.h"
#include "../../protocol.h"

HistoryDialog::HistoryDialog(ClientNetwork *net, const QString &selfUsername,
                             QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("聊天与文件历史"));
    resize(520, 450);

    auto *layout = new QVBoxLayout(this);
    auto *tabWidget = new QTabWidget(this);
    layout->addWidget(tabWidget);

    // 聊天记录标签页
    auto *chatPage = new QWidget(this);
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
    chatLayout->addWidget(new QLabel(QStringLiteral("类型："), chatPage));
    chatLayout->addWidget(chatTypeCombo);
    chatLayout->addWidget(new QLabel(QStringLiteral("目标："), chatPage));
    chatLayout->addWidget(chatTargetEdit);
    chatLayout->addWidget(chatBtn);
    chatLayout->addWidget(chatDisplay);
    tabWidget->addTab(chatPage, QStringLiteral("聊天记录"));

    // 文件记录标签页
    auto *filePage = new QWidget(this);
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
    fileLayout->addWidget(new QLabel(QStringLiteral("类型："), filePage));
    fileLayout->addWidget(fileTypeCombo);
    fileLayout->addWidget(new QLabel(QStringLiteral("目标："), filePage));
    fileLayout->addWidget(fileTargetEdit);
    fileLayout->addWidget(fileBtn);
    fileLayout->addWidget(fileDisplay);
    tabWidget->addTab(filePage, QStringLiteral("文件记录"));

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btnBox);

    // 搜索聊天
    connect(chatBtn, &QPushButton::clicked, [this, net, chatTypeCombo, chatTargetEdit, chatDisplay, selfUsername]() {
        QString type = chatTypeCombo->currentText();
        QString target = chatTargetEdit->text().trimmed();
        if (target.isEmpty()) target = selfUsername;
        chatDisplay->clear();
        chatDisplay->append(QStringLiteral("搜索中..."));

        QMetaObject::Connection *conn = new QMetaObject::Connection;
        *conn = connect(net, &ClientNetwork::messageReceived,
                        [this, net, chatDisplay, conn](const QJsonObject &msg) {
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
        net->sendHistoryQuery(type, target, 200);
    });

    // 搜索文件
    connect(fileBtn, &QPushButton::clicked, [this, net, fileTypeCombo, fileTargetEdit, fileDisplay, selfUsername]() {
        QString type = fileTypeCombo->currentText();
        QString target = fileTargetEdit->text().trimmed();
        if (target.isEmpty()) target = selfUsername;
        fileDisplay->clear();
        fileDisplay->append(QStringLiteral("搜索中..."));

        QMetaObject::Connection *conn = new QMetaObject::Connection;
        *conn = connect(net, &ClientNetwork::messageReceived,
                        [this, net, fileDisplay, conn](const QJsonObject &msg) {
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
        net->sendHistoryQuery(type == "all" ? "file" : type, target, 200);
    });
}

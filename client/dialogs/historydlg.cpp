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
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QDateTime>

#include "../../client/clientnetwork.h"
#include "../../protocol.h"

HistoryDialog::HistoryDialog(ClientNetwork *net, const QString &selfUsername,
                             QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("聊天与文件历史"));
    resize(520, 500);

    auto *layout = new QVBoxLayout(this);
    auto *tabWidget = new QTabWidget(this);
    layout->addWidget(tabWidget);

    // 时间范围控件（共用）
    auto *timeRangeWidget = new QWidget(this);
    auto *timeLayout = new QHBoxLayout(timeRangeWidget);
    timeLayout->setContentsMargins(0, 0, 0, 0);

    auto *timeCheck = new QCheckBox(QStringLiteral("按时间范围筛选"), timeRangeWidget);
    auto *startEdit = new QDateTimeEdit(timeRangeWidget);
    startEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    startEdit->setDateTime(QDateTime::currentDateTime().addMonths(-1));
    startEdit->setEnabled(false);
    auto *endEdit = new QDateTimeEdit(timeRangeWidget);
    endEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    endEdit->setDateTime(QDateTime::currentDateTime());
    endEdit->setEnabled(false);

    connect(timeCheck, &QCheckBox::toggled, startEdit, &QDateTimeEdit::setEnabled);
    connect(timeCheck, &QCheckBox::toggled, endEdit, &QDateTimeEdit::setEnabled);

    timeLayout->addWidget(timeCheck);
    timeLayout->addWidget(new QLabel(QStringLiteral("从：")));
    timeLayout->addWidget(startEdit);
    timeLayout->addWidget(new QLabel(QStringLiteral("到：")));
    timeLayout->addWidget(endEdit);

    // 聊天记录标签页
    auto *chatPage = new QWidget(this);
    auto *chatLayout = new QVBoxLayout(chatPage);
    auto *chatTypeCombo = new QComboBox(chatPage);
    chatTypeCombo->addItems({QStringLiteral("公聊记录"), QStringLiteral("私聊记录"), QStringLiteral("全部记录")});
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
    chatLayout->addWidget(timeRangeWidget);
    chatLayout->addWidget(chatBtn);
    chatLayout->addWidget(chatDisplay);
    tabWidget->addTab(chatPage, QStringLiteral("聊天记录"));

    // 文件记录标签页
    auto *filePage = new QWidget(this);
    auto *fileLayout = new QVBoxLayout(filePage);
    auto *fileTypeCombo = new QComboBox(filePage);
    fileTypeCombo->addItems({QStringLiteral("所有文件"), QStringLiteral("公有文件"), QStringLiteral("私有文件")});
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
    connect(chatBtn, &QPushButton::clicked, [this, net, chatTypeCombo, chatTargetEdit, chatDisplay, selfUsername, timeCheck, startEdit, endEdit]() {
        QString displayType = chatTypeCombo->currentText();
        QString type;
        if (displayType == QStringLiteral("公聊记录")) type = "public";
        else if (displayType == QStringLiteral("私聊记录")) type = "private";
        else type = "all";
        QString target = chatTargetEdit->text().trimmed();
        if (target.isEmpty()) target = selfUsername;
        chatDisplay->clear();
        chatDisplay->append(QStringLiteral("搜索中..."));

        QString startTime, endTime;
        if (timeCheck->isChecked()) {
            startTime = startEdit->dateTime().toString("yyyy-MM-dd HH:mm:ss");
            endTime = endEdit->dateTime().toString("yyyy-MM-dd HH:mm:ss");
        }

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
        net->sendHistoryQuery(type, target, 200, startTime, endTime);
    });

    // 搜索文件
    connect(fileBtn, &QPushButton::clicked, [this, net, fileTypeCombo, fileTargetEdit, fileDisplay, selfUsername]() {
        QString filterType = fileTypeCombo->currentText(); // 所有文件/公有文件/私有文件
        QString target = fileTargetEdit->text().trimmed();
        if (target.isEmpty()) target = selfUsername;
        fileDisplay->clear();
        fileDisplay->append(QStringLiteral("搜索中..."));

        QMetaObject::Connection *conn = new QMetaObject::Connection;
        *conn = connect(net, &ClientNetwork::messageReceived,
                        [this, net, fileDisplay, conn, filterType](const QJsonObject &msg) {
            if (msg["type"].toString() == MSG_HISTORY_RES) {
                QJsonObject data = msg["data"].toObject();
                fileDisplay->clear();
                QJsonArray allFiles = data["files"].toArray();
                QJsonArray files;
                for (const auto &f : allFiles) {
                    QJsonObject obj = f.toObject();
                    QString ft = obj["file_type"].toString();
                    if (filterType == QStringLiteral("公有文件") && ft != "public") continue;
                    if (filterType == QStringLiteral("私有文件") && ft != "private") continue;
                    files.append(f);
                }
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
        net->sendHistoryQuery("all", target, 200);
    });
}

#include "memberdlg.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QJsonObject>

void GroupMembersDialog::show(const QString &groupName, const QJsonArray &members, QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("群成员 - %1").arg(groupName));
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
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::close);
    layout->addWidget(closeBtn, 0, Qt::AlignCenter);

    dlg.exec();
}

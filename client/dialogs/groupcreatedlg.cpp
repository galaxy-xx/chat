#include "groupcreatedlg.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QMessageBox>

CreateGroupDialog::CreateGroupDialog(const QStringList &friends, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("创建群聊"));
    resize(280, 320);

    auto *layout = new QVBoxLayout(this);

    QString name = QInputDialog::getText(this, QStringLiteral("创建群聊"),
                                          QStringLiteral("群名称："));
    if (name.isEmpty()) {
        reject();
        return;
    }
    m_result.name = name;

    layout->addWidget(new QLabel(QStringLiteral("请选择要加入群聊的好友："), this));

    auto *listWidget = new QListWidget(this);
    for (const auto &f : friends) {
        auto *item = new QListWidgetItem(f);
        item->setCheckState(Qt::Checked);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        listWidget->addItem(item);
    }
    listWidget->setStyleSheet("QListWidget::item { padding: 6px; }");
    layout->addWidget(listWidget);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, this, [this, listWidget]() {
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
        m_result.members = members;
        m_result.accepted = true;
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

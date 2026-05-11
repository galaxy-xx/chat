#ifndef GROUPMEMBERSDIALOG_H
#define GROUPMEMBERSDIALOG_H

#include <QDialog>
#include <QJsonArray>

namespace GroupMembersDialog {
    void show(const QString &groupName, const QJsonArray &members, QWidget *parent = nullptr);
}

#endif // GROUPMEMBERSDIALOG_H

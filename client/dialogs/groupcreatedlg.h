#ifndef CREATEGROUPDIALOG_H
#define CREATEGROUPDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QString>

struct GroupCreateResult {
    bool accepted = false;
    QString name;
    QStringList members;
};

class CreateGroupDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CreateGroupDialog(const QStringList &friends, QWidget *parent = nullptr);

    GroupCreateResult result() const { return m_result; }

private:
    GroupCreateResult m_result;
};

#endif // CREATEGROUPDIALOG_H

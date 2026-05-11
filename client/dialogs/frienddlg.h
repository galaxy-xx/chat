#ifndef FRIENDMANAGEMENTDIALOG_H
#define FRIENDMANAGEMENTDIALOG_H

#include <QDialog>
#include <QStringList>
#include <functional>

class ClientNetwork;

class FriendManagementDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FriendManagementDialog(ClientNetwork *net,
                                    const QStringList &friends,
                                    const QStringList &pendingIncoming,
                                    const QStringList &pendingOutgoing,
                                    const QStringList &onlineUsers,
                                    const QString &selfUsername,
                                    QWidget *parent = nullptr);

    void setOnChanged(std::function<void()> callback) { m_onChanged = callback; }

private:
    void populateList();

    ClientNetwork *m_net;
    QStringList m_friends;
    QStringList m_pendingIncoming;
    QStringList m_pendingOutgoing;
    QStringList m_onlineUsers;
    QString m_selfUsername;
    std::function<void()> m_onChanged;
    class QListWidget *m_listWidget = nullptr;
};

#endif // FRIENDMANAGEMENTDIALOG_H

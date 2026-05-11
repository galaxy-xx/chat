#ifndef HISTORYDIALOG_H
#define HISTORYDIALOG_H

#include <QDialog>

class ClientNetwork;

class HistoryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HistoryDialog(ClientNetwork *net, const QString &selfUsername,
                           QWidget *parent = nullptr);
};

#endif // HISTORYDIALOG_H

#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);

    QString username() const;
    QString password() const;
    void setStatus(const QString &msg);
    void setButtonsEnabled(bool enabled);

private slots:
    void onLoginClicked();
    void onRegisterClicked();

signals:
    void loginRequested(const QString &user, const QString &pass);
    void registerRequested(const QString &user, const QString &pass);

private:
    QLineEdit *m_userEdit;
    QLineEdit *m_passEdit;
    QPushButton *m_loginBtn;
    QPushButton *m_registerBtn;
    QLabel *m_statusLabel;
};

#endif // LOGINDIALOG_H

#include "logindialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("登录"));
    setFixedSize(380, 400);
    setObjectName("loginDialog");

    // 整对话框样式（覆盖全局样式表的干扰）
    setStyleSheet(QStringLiteral(R"(
        #loginDialog { background: white; }
        #loginDialog QLineEdit {
            border: 1px solid #D9D9D9;
            border-radius: 6px;
            padding: 11px 14px;
            font-size: 15px;
            background: #F7F7F7;
            color: #353535;
            min-height: 20px;
        }
        #loginDialog QLineEdit:focus {
            border-color: #07C160;
            background: white;
        }
        #loginDialog QPushButton#loginBtn {
            background: #07C160; color: white; border: none;
            border-radius: 22px; padding: 12px;
            font-size: 16px; font-weight: bold;
        }
        #loginDialog QPushButton#loginBtn:hover { background: #06AD56; }
        #loginDialog QPushButton#loginBtn:pressed { background: #059B4C; }
        #loginDialog QPushButton#loginBtn:disabled { background: #BBBBBB; color: #EEEEEE; }
        #loginDialog QPushButton#registerBtn {
            background: transparent; color: #888888; border: none;
            font-size: 13px;
        }
        #loginDialog QPushButton#registerBtn:hover { color: #07C160; }
        #loginDialog QPushButton#registerBtn:disabled { color: #CCCCCC; }
    )"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 25, 40, 25);
    mainLayout->setSpacing(0);

    // ── 顶部圆形图标 ──
    auto *iconWidget = new QWidget(this);
    iconWidget->setFixedSize(60, 60);
    iconWidget->setStyleSheet(
        "background: #07C160; border-radius: 30px;");
    auto *iconLayout = new QVBoxLayout(iconWidget);
    iconLayout->setAlignment(Qt::AlignCenter);
    auto *iconLabel = new QLabel(QStringLiteral("信"), iconWidget);
    iconLabel->setStyleSheet(
        "color: white; font-size: 28px; font-weight: bold; background: transparent;");
    iconLayout->addWidget(iconLabel);
    mainLayout->addWidget(iconWidget, 0, Qt::AlignCenter);
    mainLayout->addSpacing(12);

    // ── 标题 ──
    auto *titleLabel = new QLabel(QStringLiteral("微信"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "font-size: 20px; font-weight: bold; color: #353535; background: transparent;");
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(24);

    // ── 用户名输入框 ──
    m_userEdit = new QLineEdit(this);
    m_userEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    m_userEdit->setMaxLength(32);
    mainLayout->addWidget(m_userEdit);
    mainLayout->addSpacing(14);

    // ── 密码输入框 ──
    m_passEdit = new QLineEdit(this);
    m_passEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    m_passEdit->setEchoMode(QLineEdit::Password);
    mainLayout->addWidget(m_passEdit);
    mainLayout->addSpacing(18);

    // ── 登录按钮 ──
    m_loginBtn = new QPushButton(QStringLiteral("登 录"), this);
    m_loginBtn->setObjectName("loginBtn");
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    m_loginBtn->setFixedHeight(44);
    mainLayout->addWidget(m_loginBtn);
    mainLayout->addSpacing(10);

    // ── 注册按钮 ──
    m_registerBtn = new QPushButton(QStringLiteral("注册账号"), this);
    m_registerBtn->setObjectName("registerBtn");
    m_registerBtn->setCursor(Qt::PointingHandCursor);
    mainLayout->addWidget(m_registerBtn, 0, Qt::AlignCenter);

    // ── 状态提示 ──
    mainLayout->addSpacing(4);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("color: #FA5151; font-size: 12px; background: transparent;");
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch();

    // ── 连接信号 ──
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(m_registerBtn, &QPushButton::clicked, this, &LoginDialog::onRegisterClicked);
    connect(m_passEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
}

QString LoginDialog::username() const { return m_userEdit->text().trimmed(); }
QString LoginDialog::password() const { return m_passEdit->text(); }

void LoginDialog::onLoginClicked()
{
    if (username().isEmpty() || password().isEmpty()) {
        m_statusLabel->setText(QStringLiteral("请输入用户名和密码"));
        return;
    }
    m_loginBtn->setEnabled(false);
    m_registerBtn->setEnabled(false);
    emit loginRequested(username(), password());
}

void LoginDialog::onRegisterClicked()
{
    if (username().isEmpty() || password().isEmpty()) {
        m_statusLabel->setText(QStringLiteral("请输入用户名和密码"));
        return;
    }
    m_loginBtn->setEnabled(false);
    m_registerBtn->setEnabled(false);
    emit registerRequested(username(), password());
}

void LoginDialog::setStatus(const QString &msg)
{
    m_statusLabel->setText(msg);
}

void LoginDialog::setButtonsEnabled(bool enabled)
{
    m_loginBtn->setEnabled(enabled);
    m_registerBtn->setEnabled(enabled);
}

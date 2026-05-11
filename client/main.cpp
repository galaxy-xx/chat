#include <QApplication>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QProcess>
#include <QDir>
#include <QFileInfo>

#include "clientnetwork.h"
#include "logindialog.h"
#include "mainwindow.h"

// 查找并启动服务器
static QProcess* startServerProcess()
{
    QStringList candidates = {
        QApplication::applicationDirPath() + "/chat_server",
        QApplication::applicationDirPath() + "/../chat_server",
        QApplication::applicationDirPath() + "/../../chat_server",
        QDir::currentPath() + "/chat_server"
    };

    for (const auto &p : candidates) {
        QFileInfo fi(p);
        if (fi.exists() && fi.isExecutable()) {
            auto *proc = new QProcess();
            proc->setWorkingDirectory(fi.absolutePath());
            proc->start(fi.absoluteFilePath(), {"-f"});
            return proc;
        }
    }
    return nullptr;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("WeChat"));

    // ── 微信风格全局样式表 ──
    app.setStyleSheet(QStringLiteral(R"(
        /* 全局默认 */
        * {
            font-family: "Microsoft YaHei", "PingFang SC", "Noto Sans CJK SC", sans-serif;
            font-size: 14px;
            color: #353535;
        }
        QMainWindow, QDialog {
            background: #F7F7F7;
        }
        /* 按钮 */
        QPushButton {
            background: #07C160;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 8px 20px;
            font-size: 14px;
            min-height: 20px;
        }
        QPushButton:hover {
            background: #06AD56;
        }
        QPushButton:pressed {
            background: #059B4C;
        }
        QPushButton:disabled {
            background: #BBBBBB;
            color: #EEEEEE;
        }
        /* 输入框 */
        QLineEdit {
            border: 1px solid #D9D9D9;
            border-radius: 4px;
            padding: 8px 12px;
            font-size: 14px;
            background: white;
            color: #353535;
        }
        QLineEdit:focus {
            border-color: #07C160;
        }
        /* 列表 */
        QListWidget {
            border: none;
            background: #F7F7F7;
            font-size: 14px;
            outline: none;
        }
        QListWidget::item {
            padding: 10px 15px;
            border-bottom: 1px solid #EBEBEB;
            color: #353535;
        }
        QListWidget::item:hover {
            background: #E8E8E8;
        }
        QListWidget::item:selected {
            background: #D9D9D9;
        }
        /* 标签页 */
        QTabWidget::pane {
            border: none;
            background: #EDEDED;
        }
        QTabBar::tab {
            background: #F7F7F7;
            color: #888888;
            padding: 8px 16px;
            border: none;
            border-bottom: 2px solid transparent;
            min-width: 60px;
        }
        QTabBar::tab:selected {
            border-bottom: 2px solid #07C160;
            color: #07C160;
        }
        QTabBar::tab:hover {
            color: #07C160;
        }
        /* 文本编辑区 */
        QTextEdit {
            border: 1px solid #D9D9D9;
            border-radius: 4px;
            padding: 8px;
            font-size: 14px;
            background: white;
            color: #353535;
        }
        QTextEdit:focus {
            border-color: #07C160;
        }
        /* 菜单栏 */
        QMenuBar {
            background: #F7F7F7;
            border-bottom: 1px solid #EBEBEB;
            padding: 2px;
        }
        QMenuBar::item {
            padding: 4px 12px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background: #E8E8E8;
        }
        QMenu {
            background: white;
            border: 1px solid #D9D9D9;
            padding: 4px 0;
        }
        QMenu::item {
            padding: 6px 24px;
        }
        QMenu::item:selected {
            background: #07C160;
            color: white;
        }
        /* 滚动条 */
        QScrollBar:vertical {
            width: 6px;
            background: transparent;
        }
        QScrollBar::handle:vertical {
            background: #C0C0C0;
            border-radius: 3px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #A0A0A0;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        /* 消息提示框 */
        QMessageBox {
            background: white;
        }
        QMessageBox QPushButton {
            min-width: 60px;
        }
        /* 对话框按钮 */
        QDialogButtonBox QPushButton {
            min-width: 60px;
        }
    )"));

    QString serverHost = "127.0.0.1";
    quint16 serverPort = SERVER_PORT;
    if (argc > 1) serverHost = argv[1];
    if (argc > 2) serverPort = atoi(argv[2]);

    ClientNetwork net;
    LoginDialog loginDlg;
    QProcess *serverProc = nullptr;
    bool authenticated = false;
    QString loggedInUser;

    // ── 处理服务器返回的登录/注册结果 ──
    QObject::connect(&net, &ClientNetwork::messageReceived,
        [&](const QJsonObject &msg) {
            QString type = msg["type"].toString();
            QJsonObject data = msg["data"].toObject();

            if (type == MSG_LOGIN_RES || type == MSG_REGISTER_RES) {
                bool ok = data["ok"].toBool();
                QString message = data["message"].toString();

                if (ok && type == MSG_LOGIN_RES) {
                    authenticated = true;
                    loggedInUser = loginDlg.username();
                    loginDlg.accept();
                } else if (ok && type == MSG_REGISTER_RES) {
                    QMessageBox::information(&loginDlg, QStringLiteral("成功"),
                        QStringLiteral("注册成功，请登录！"));
                    loginDlg.setStatus(QStringLiteral("注册成功，请登录"));
                    loginDlg.setButtonsEnabled(true);
                } else {
                    loginDlg.setStatus(message);
                    loginDlg.setButtonsEnabled(true);
                }
            }
        });

    // ── 连接后的自动发送 ──
    bool pendingRegister = false;
    QObject::connect(&net, &ClientNetwork::connected, [&]() {
        loginDlg.setStatus(QStringLiteral("已连接，正在验证..."));
        if (pendingRegister)
            net.sendRegister(loginDlg.username(), loginDlg.password());
        else
            net.sendLogin(loginDlg.username(), loginDlg.password());
    });

    // ── 发起连接（点击登录/注册时调用） ──
    bool connecting = false;
    auto doConnect = [&](bool isRegister) {
        connecting = true;
        pendingRegister = isRegister;
        loginDlg.setStatus(QStringLiteral("正在连接服务器..."));
        net.connectToServer(serverHost, serverPort);
    };

    // ── 按钮事件 ──
    QObject::connect(&loginDlg, &LoginDialog::loginRequested,
        [&](const QString &, const QString &) {
            if (!net.isConnected() && !connecting)
                doConnect(false);
            else if (net.isConnected())
                net.sendLogin(loginDlg.username(), loginDlg.password());
        });

    QObject::connect(&loginDlg, &LoginDialog::registerRequested,
        [&](const QString &, const QString &) {
            if (!net.isConnected() && !connecting)
                doConnect(true);
            else if (net.isConnected())
                net.sendRegister(loginDlg.username(), loginDlg.password());
        });

    // ── 连接错误 → 尝试启动服务器 ──
    QObject::connect(&net, &ClientNetwork::connectionError,
        [&](const QString &) {
            if (!connecting) return;
            connecting = false;

            loginDlg.setStatus(QStringLiteral("正在启动服务器..."));
            if (!serverProc || serverProc->state() == QProcess::NotRunning) {
                delete serverProc;
                serverProc = startServerProcess();
            }

            if (!serverProc || serverProc->state() == QProcess::NotRunning) {
                loginDlg.setStatus(QStringLiteral("无法连接服务器，请手动启动 chat_server"));
                loginDlg.setButtonsEnabled(true);
                return;
            }

            loginDlg.setStatus(QStringLiteral("服务器启动中，请稍候..."));
            QTimer::singleShot(2000, [&]() {
                if (!net.isConnected()) {
                    connecting = true;
                    loginDlg.setStatus(QStringLiteral("正在重试连接..."));
                    net.connectToServer(serverHost, serverPort);

                    QTimer::singleShot(3000, [&]() {
                        if (!net.isConnected()) {
                            connecting = false;
                            loginDlg.setStatus(QStringLiteral("连接服务器失败，请检查 chat_server"));
                            loginDlg.setButtonsEnabled(true);
                        }
                    });
                }
            });
        });

    // ── 运行登录对话框 ──
    if (loginDlg.exec() != QDialog::Accepted || !authenticated) {
        net.disconnect();
        return 0;
    }

    // ── 进入主界面 ──
    MainWindow *mainWin = new MainWindow(&net, loggedInUser);
    mainWin->show();
    net.sendUserList();

    int ret = app.exec();
    delete mainWin;
    if (serverProc) {
        serverProc->terminate();
        if (!serverProc->waitForFinished(3000))
            serverProc->kill();
        delete serverProc;
    }
    return ret;
}

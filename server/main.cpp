#include <QCoreApplication>
#include <QDir>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "database.h"
#include "server.h"
#include "../protocol.h"

static void daemonize()
{
    pid_t pid = fork();
    if (pid < 0) {
        qCritical("fork failed");
        exit(1);
    }
    if (pid > 0) exit(0);

    setsid();
    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("chat-server");

    bool foreground = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--foreground") == 0 || strcmp(argv[i], "-f") == 0)
            foreground = true;
    }
    if (!foreground) daemonize();

    Database db;
    QString dbPath = QDir::homePath() + "/.chat_server.db";
    if (!db.open(dbPath)) {
        qCritical("无法打开数据库");
        return 1;
    }

    QString storageDir = QDir::homePath() + "/.chat_files";
    QDir().mkpath(storageDir);

    ChatServer server(&db, storageDir);
    if (!server.start(SERVER_PORT))
        return 1;

    qInfo() << "Chat server running (foreground:" << foreground << ")";
    return app.exec();
}

#include "chat.h"

#include <QLabel>
#include <QScrollBar>
#include <QTimer>
#include <QPixmap>
#include <QMouseEvent>

ChatWidget::ChatWidget(const QString &chatWith, QWidget *parent)
    : QWidget(parent), m_chatWith(chatWith)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(
        "QScrollArea { background: #EDEDED; border: none; }");

    m_contentWidget = new QWidget(this);
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(12, 12, 12, 12);
    m_contentLayout->setSpacing(2);
    m_contentLayout->addStretch();

    m_scrollArea->setWidget(m_contentWidget);
    layout->addWidget(m_scrollArea);
}

bool ChatWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto *label = qobject_cast<QLabel*>(obj)) {
            QString fpath = label->property("imagePath").toString();
            if (!fpath.isEmpty()) {
                emit imageClicked(fpath);
                return true;
            }
        }
    }
    if (event->type() == QEvent::ContextMenu) {
        if (auto *bubble = qobject_cast<BubbleWidget*>(obj)) {
            if (bubble->isSelf()) {
                emit bubbleRightClicked(bubble);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ChatWidget::appendMessage(const QString &from, const QString &content,
                                const QString &time, int msgId)
{
    bool isSelf = (from == QStringLiteral("我"));

    if (!isSelf) {
        auto *nameLabel = new QLabel(from, m_contentWidget);
        nameLabel->setStyleSheet(
            "color: #888888; font-size: 12px; padding: 4px 60px 0 60px; background: transparent;");
        nameLabel->setAlignment(Qt::AlignLeft);
        m_contentLayout->insertWidget(m_contentLayout->count() - 1, nameLabel);
    }

    auto *bubble = new BubbleWidget(content, time, isSelf, msgId, m_contentWidget);
    bubble->installEventFilter(this);
    Qt::Alignment align = isSelf ? Qt::AlignRight : Qt::AlignLeft;
    m_contentLayout->insertWidget(m_contentLayout->count() - 1, bubble, 0, align);

    if (msgId > 0)
        m_msgMap.insert(msgId, bubble);

    QTimer::singleShot(50, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum());
    });
}

void ChatWidget::appendSystemMessage(const QString &msg)
{
    auto *sysLabel = new QLabel(msg, m_contentWidget);
    sysLabel->setAlignment(Qt::AlignCenter);
    sysLabel->setStyleSheet(
        "color: #B0B0B0; font-size: 12px; padding: 8px; background: transparent;");
    sysLabel->setWordWrap(true);
    m_contentLayout->insertWidget(m_contentLayout->count() - 1, sysLabel);

    QTimer::singleShot(50, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum());
    });
}

void ChatWidget::appendImageMessage(const QString &from, const QString &filepath,
                                     const QString &filename, const QString &time)
{
    bool isSelf = (from == QStringLiteral("我"));

    if (!isSelf) {
        auto *nameLabel = new QLabel(from, m_contentWidget);
        nameLabel->setStyleSheet(
            "color: #888888; font-size: 12px; padding: 4px 60px 0 60px; background: transparent;");
        nameLabel->setAlignment(Qt::AlignLeft);
        m_contentLayout->insertWidget(m_contentLayout->count() - 1, nameLabel);
    }

    QPixmap pix(filepath);
    auto *imgLabel = new QLabel(m_contentWidget);
    if (!pix.isNull()) {
        QPixmap thumb = pix.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        imgLabel->setPixmap(thumb);
    }
    imgLabel->setCursor(Qt::PointingHandCursor);
    imgLabel->setToolTip(QStringLiteral("点击预览: %1").arg(filename));
    imgLabel->setProperty("imagePath", filepath);
    imgLabel->setStyleSheet("border-radius: 4px; padding: 4px; background: transparent;");
    imgLabel->installEventFilter(this);

    int marginSide = isSelf ? 60 : 12;
    int marginOther = isSelf ? 12 : 60;

    auto *container = new QWidget(m_contentWidget);
    auto *clayout = new QVBoxLayout(container);
    clayout->setContentsMargins(marginOther, 2, marginSide, 2);
    clayout->setSpacing(2);

    auto *timeLabel = new QLabel(time, container);
    timeLabel->setStyleSheet("color: #B0B0B0; font-size: 11px; background: transparent;");
    timeLabel->setAlignment(isSelf ? Qt::AlignRight : Qt::AlignLeft);
    clayout->addWidget(timeLabel);
    clayout->addWidget(imgLabel, 0, isSelf ? Qt::AlignRight : Qt::AlignLeft);

    container->setStyleSheet("background: transparent;");
    m_contentLayout->insertWidget(m_contentLayout->count() - 1, container);

    QTimer::singleShot(50, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum());
    });
}

void ChatWidget::appendFileMessage(const QString &from, const QString &filename,
                                    qint64 filesize, const QString &time)
{
    bool isSelf = (from == QStringLiteral("我"));

    if (!isSelf) {
        auto *nameLabel = new QLabel(from, m_contentWidget);
        nameLabel->setStyleSheet(
            "color: #888888; font-size: 12px; padding: 4px 60px 0 60px; background: transparent;");
        nameLabel->setAlignment(Qt::AlignLeft);
        m_contentLayout->insertWidget(m_contentLayout->count() - 1, nameLabel);
    }

    QString sizeStr;
    if (filesize < 1024)
        sizeStr = QStringLiteral("%1 B").arg(filesize);
    else if (filesize < 1024 * 1024)
        sizeStr = QStringLiteral("%1 KB").arg(filesize / 1024);
    else
        sizeStr = QStringLiteral("%1 MB").arg(filesize / (1024 * 1024));

    auto *container = new QWidget(m_contentWidget);
    container->setStyleSheet("background: transparent;");
    auto *clayout = new QVBoxLayout(container);
    clayout->setContentsMargins(12, 2, 12, 2);
    clayout->setSpacing(2);

    auto *timeLabel = new QLabel(time, container);
    timeLabel->setStyleSheet("color: #B0B0B0; font-size: 11px; background: transparent;");
    timeLabel->setAlignment(isSelf ? Qt::AlignRight : Qt::AlignLeft);
    clayout->addWidget(timeLabel);

    auto *fileLabel = new QLabel(
        QStringLiteral("📎 %1\n%2").arg(filename, sizeStr), container);
    fileLabel->setWordWrap(true);
    fileLabel->setMaximumWidth(300);
    fileLabel->setCursor(Qt::PointingHandCursor);
    fileLabel->setStyleSheet(
        "background: white; color: #353535; font-size: 13px;"
        "padding: 10px 14px; border-radius: 6px;");
    fileLabel->setAlignment(isSelf ? Qt::AlignRight : Qt::AlignLeft);
    clayout->addWidget(fileLabel, 0, isSelf ? Qt::AlignRight : Qt::AlignLeft);

    m_contentLayout->insertWidget(m_contentLayout->count() - 1, container);

    QTimer::singleShot(50, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum());
    });
}

bool ChatWidget::removeMessage(int msgId)
{
    if (!m_msgMap.contains(msgId)) return false;
    BubbleWidget *bubble = m_msgMap.take(msgId);
    bubble->markRecalled();
    return true;
}

void ChatWidget::updateLastMsgId(int msgId)
{
    for (int i = m_contentLayout->count() - 1; i >= 0; --i) {
        QWidget *w = m_contentLayout->itemAt(i)->widget();
        if (auto *bubble = qobject_cast<BubbleWidget*>(w)) {
            if (bubble->msgId() == -1) {
                bubble->setMsgId(msgId);
                m_msgMap.insert(msgId, bubble);
                break;
            }
        }
    }
}

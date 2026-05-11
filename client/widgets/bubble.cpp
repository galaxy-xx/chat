#include "bubble.h"

#include <QGraphicsDropShadowEffect>

BubbleWidget::BubbleWidget(const QString &content, const QString &time,
                           bool isSelf, int msgId, QWidget *parent)
    : QFrame(parent), m_msgId(msgId), m_isSelf(isSelf)
{
    QString bgColor = isSelf ? "#95EC69" : "#FFFFFF";
    int marginRight = isSelf ? 60 : 12;
    int marginLeft  = isSelf ? 12 : 60;

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(marginLeft, 3, marginRight, 3);
    layout->setSpacing(3);

    auto *timeLabel = new QLabel(time, this);
    timeLabel->setStyleSheet("color: #B0B0B0; font-size: 11px; padding: 0; background: transparent;");
    timeLabel->setAlignment(isSelf ? Qt::AlignRight : Qt::AlignLeft);

    m_bubbleLabel = new QLabel(content, this);
    m_bubbleLabel->setWordWrap(true);
    m_bubbleLabel->setMaximumWidth(420);
    m_bubbleLabel->setStyleSheet(QString(
        "background: %1; color: #353535; font-size: 14px;"
        "padding: 10px 14px; border-radius: 6px;"
    ).arg(bgColor));
    m_bubbleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    auto *shadow = new QGraphicsDropShadowEffect(m_bubbleLabel);
    shadow->setBlurRadius(8);
    shadow->setOffset(0, 1);
    shadow->setColor(QColor(0, 0, 0, 30));
    m_bubbleLabel->setGraphicsEffect(shadow);

    layout->addWidget(timeLabel);
    layout->addWidget(m_bubbleLabel, 0, isSelf ? Qt::AlignRight : Qt::AlignLeft);

    setStyleSheet("background: transparent;");
}

void BubbleWidget::markRecalled()
{
    if (m_bubbleLabel)
        m_bubbleLabel->setStyleSheet(
            "background: #E8E8E8; color: #B0B0B0; font-size: 13px;"
            "padding: 10px 14px; border-radius: 6px;");
    m_bubbleLabel->setText(QStringLiteral("[消息已撤回]"));
}

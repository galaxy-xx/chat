#ifndef BUBBLEWIDGET_H
#define BUBBLEWIDGET_H

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

class BubbleWidget : public QFrame
{
    Q_OBJECT
public:
    explicit BubbleWidget(const QString &content, const QString &time,
                          bool isSelf, int msgId = -1,
                          QWidget *parent = nullptr);
    int msgId() const { return m_msgId; }
    bool isSelf() const { return m_isSelf; }
    void setMsgId(int id) { m_msgId = id; }
    void markRecalled();

private:
    QLabel *m_bubbleLabel = nullptr;
    int m_msgId;
    bool m_isSelf;
};

#endif // BUBBLEWIDGET_H

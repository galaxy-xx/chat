#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QMap>
#include <QString>

#include "bubble.h"

class ChatWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChatWidget(const QString &chatWith, QWidget *parent = nullptr);

    void appendMessage(const QString &from, const QString &content,
                       const QString &time, int msgId = -1);
    void appendSystemMessage(const QString &msg);
    void appendImageMessage(const QString &from, const QString &filepath,
                            const QString &filename, const QString &time);
    void appendFileMessage(const QString &from, const QString &filename,
                           qint64 filesize, const QString &time);
    bool removeMessage(int msgId);
    void updateLastMsgId(int msgId);
    QString chatWith() const { return m_chatWith; }

signals:
    void imageClicked(const QString &filepath);
    void fileClicked(const QString &filename, const QString &base64Data);
    void bubbleRightClicked(BubbleWidget *bubble);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QScrollArea *m_scrollArea;
    QWidget     *m_contentWidget;
    QVBoxLayout *m_contentLayout;
    QString m_chatWith;
    QMap<int, BubbleWidget*> m_msgMap;
};

#endif // CHATWIDGET_H

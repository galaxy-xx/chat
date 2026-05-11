#ifndef IMAGEPREVIEWDIALOG_H
#define IMAGEPREVIEWDIALOG_H

#include <QWidget>
#include <QString>

// 图片预览工具函数
namespace ImagePreviewDialog {
    void show(const QString &filepath, QWidget *parent = nullptr);
}

#endif // IMAGEPREVIEWDIALOG_H

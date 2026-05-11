#include "imagedlg.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

void ImagePreviewDialog::show(const QString &filepath, QWidget *parent)
{
    QPixmap pix(filepath);
    if (pix.isNull()) {
        QMessageBox::warning(parent, QStringLiteral("预览失败"),
                             QStringLiteral("无法加载图片"));
        return;
    }

    auto *dlg = new QDialog(parent);
    dlg->setWindowTitle(QStringLiteral("图片预览"));
    dlg->resize(600, 500);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto *layout = new QVBoxLayout(dlg);
    auto *label = new QLabel(dlg);
    label->setPixmap(pix.scaled(560, 440, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("background: #1E1E1E; border-radius: 4px; padding: 10px;");
    layout->addWidget(label);

    auto *btnLayout = new QHBoxLayout;
    auto *openBtn = new QPushButton(QStringLiteral("打开原图"), dlg);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), dlg);
    btnLayout->addStretch();
    btnLayout->addWidget(openBtn);
    btnLayout->addWidget(closeBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    QObject::connect(openBtn, &QPushButton::clicked, [filepath]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filepath));
    });
    QObject::connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);

    dlg->show();
}

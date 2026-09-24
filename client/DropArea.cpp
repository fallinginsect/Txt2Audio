#include "DropArea.h"
#include <QUrl>
#include <QMimeData>
#include <QFileDialog>

DropArea::DropArea(QWidget *parent) : QWidget(parent) {
    setAcceptDrops(true);
    setMinimumHeight(120);
    setAttribute(Qt::WA_StyledBackground, true);
    updateStyle(false);
}

void DropArea::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        if (event->mimeData()->urls().isEmpty()) return;
        QString path = event->mimeData()->urls().first().toLocalFile();
        if (path.endsWith(".txt", Qt::CaseInsensitive)) {
            event->acceptProposedAction();
            updateStyle(true);
        }
    }
}

void DropArea::dragLeaveEvent(QDragLeaveEvent *event) {
    updateStyle(false);
}

void DropArea::dropEvent(QDropEvent *event) {
    if (event->mimeData()->urls().isEmpty()) {
        updateStyle(false);
        return;
    }
    QString path = event->mimeData()->urls().first().toLocalFile();
    emit fileSelected(path);
    updateStyle(false);
}

void DropArea::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    QString path = QFileDialog::getOpenFileName(
        this, "选择文本文件", "", "文本文件 (*.txt);;所有文件 (*.*)"
        );
    if (!path.isEmpty()) {
        emit fileSelected(path);
    }
}

void DropArea::updateStyle(bool hover) {
    if (hover) {
        // 拖拽高亮：蓝色实线边框 + 深蓝半透明背景
        setStyleSheet(R"(
            QWidget {
                border: 2px solid #3B82F6;
                border-radius: 8px;
                background-color: #1E293B;
            }
        )");
    } else {
        // 默认状态：浅灰蓝色虚线边框，深色背景下清晰可见
        setStyleSheet(R"(
            QWidget {
                border: 2px dashed #94A3B8;
                border-radius: 8px;
                background-color: #1F1F1F;
            }
        )");
    }
}

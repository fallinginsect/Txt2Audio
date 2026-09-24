#ifndef DROPAREA_H
#define DROPAREA_H

#include <QWidget>
#include <QObject>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMouseEvent>

// 头文件 droparea.h
class DropArea : public QWidget {
    Q_OBJECT
public:
    explicit DropArea(QWidget *parent = nullptr);
signals:
    void fileSelected(const QString &filePath); // 选中文件后发出信号
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
private:
    void updateStyle(bool hover); // 拖拽状态样式切换
};

#endif // DROPAREA_H

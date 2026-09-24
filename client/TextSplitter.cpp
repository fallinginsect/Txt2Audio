#include "TextSplitter.h"
#include <QString>
#include <QVector>
#include <QChar>

QVector<QString> TextSplitter::split(const QString& text, int maxLen) {
    QVector<QString> chunks;
    if (text.isEmpty()) return chunks;

    // Qt 的 QString 直接支持中文标点，每个标点就是一个 QChar
    QString delimiters = QStringLiteral("。；！？\n；.~……");
    int start = 0;
    int totalLen = text.length(); // 注意：QString::length() 返回的是字符数（UTF-16 单元数），不是字节数

    while (start < totalLen) {
        int end = text.indexOf(delimiters, start);

        if (end == -1) {
            end = totalLen;
        } else {
            // 吞掉连续标点（现在可以直接用 QChar 比较）
            while (end + 1 < totalLen && delimiters.contains(text[end + 1])) {
                end++;
            }
            end += 1; // 包含最后一个标点
        }

        int len = end - start;

        // 长度超限，尝试回退到空格（Qt 里也是直接找 QChar 空格）
        if (len > maxLen) {
            int spacePos = text.lastIndexOf(u' ', start + maxLen);
            if (spacePos > start && spacePos < end) {
                end = spacePos + 1;
            } else {
                end = start + maxLen;
            }
        }

        // 安全保护
        if (end <= start) {
            end = qMin(start + maxLen, totalLen);
        }

        chunks.append(text.mid(start, end - start));
        start = end;
    }
    return chunks;
}
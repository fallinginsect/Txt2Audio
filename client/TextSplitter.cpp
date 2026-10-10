#include "TextSplitter.h"
#include <QString>
#include <QVector>
#include <QChar>

static int indexOfAny(const QString &str, const QString &chars, int from = 0) {
    int minPos = -1;
    for (QChar ch : chars)
    {
        int pos = str.indexOf(ch, from);
        if (pos != -1 && (minPos == -1 || pos < minPos))
            minPos = pos;
    }
    return minPos;
}


QVector<QString> TextSplitter::split(const QString& text, int maxLen) {
    QVector<QString> chunks;
    if (text.isEmpty()) return chunks;

    QString delimiters = QStringLiteral("。;！？\n；.~…");
    int start = 0;
    const int totalLen = text.length();

    while (start < totalLen) {
        int hardEnd = qMin(start + maxLen, totalLen);

        // 记录"最后一个不超过 maxLen 的分隔符位置"
        int lastCut = -1;
        int searchFrom = start;

        while (true) {
            int pos = indexOfAny(text, delimiters, searchFrom);
            if (pos == -1 || pos >= hardEnd) break;   // 没找到 或 越界了
            lastCut = pos;                             // 更新候选
            searchFrom = pos + 1;                      // 从下一个位置继续找
        }

        int end;
        if (lastCut == -1) {
            // 区间内没分隔符，硬切
            end = hardEnd;
        } else {
            // 在 lastCut 处切，吞掉连续标点
            end = lastCut + 1;
            while (end < totalLen && delimiters.contains(text[end])) {
                end++;
            }
        }

        if (end <= start) {
            end = qMin(start + maxLen, totalLen);
        }

        QString chunk = text.mid(start, end - start);
        if (!chunk.trimmed().isEmpty()) {
            chunks.append(chunk);
        }
        start = end;
    }
    return chunks;
}
#ifndef TEXTSPLITTER_H
#define TEXTSPLITTER_H

#include <QVector>
#include <QString>

class TextSplitter {
public:
    static QVector<QString> split(const QString& text, int maxLen = 4000);
};

#endif // TEXTSPLITTER_H

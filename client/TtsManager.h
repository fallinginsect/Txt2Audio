#ifndef TTSMANAGER_H
#define TTSMANAGER_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QMap>
#include "TtsClient.h"

class TtsManager : public QObject {
    Q_OBJECT
public:
    // 支持传入服务端地址，默认和TtsClient保持一致
    explicit TtsManager(QObject* parent = nullptr, const QString& baseUrl = "http://127.0.0.1:8000");

    //和声明一致，支持传入 speaker_id 和 speed
    void synthesizeFile(const QString& inputFile, const QString& outputDir,
                        int speaker_id = 1, double speed = 1.0);

signals:
    void progressUpdated(int current, int total);
    void synthesisFinished(const QString& outputFile);
    void errorOccurred(const QString& msg);

private:
    void sendAllChunks(int speaker_id, double speed);
    void mergeAll();
    QString getTempFilePath(int index) const;
    void cleanup();

    QVector<QString> chunks;
    QMap<TtsClient*, int> m_clientMap; // 跟踪每个客户端对应的分片序号
    int totalChunks;
    int receivedCount;
    bool failed;
    QString tempDir;
    QString outputFilePath; // 最终输出文件路径（成员变量，避免局部屏蔽）
    QString m_baseUrl;      // 服务端地址
};

#endif // TTSMANAGER_H

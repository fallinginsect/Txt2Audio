#include "TtsManager.h"
#include "TextSplitter.h"
#include "WavUtils.h"
#include <QFile>
#include <QDir>
#include <QStringList>
#include <QDebug>

TtsManager::TtsManager(QObject* parent, const QString& baseUrl)
    : QObject(parent)
    , totalChunks(0)
    , receivedCount(0)
    , failed(false)
    , m_baseUrl(baseUrl)
{
    // 不再自己创建QNetworkAccessManager，所有网络请求全部委托给TtsClient
}

void TtsManager::synthesizeFile(const QString& inputFile, const QString& outputDir,
                                int speaker_id, double speed)
{
    //  每次新任务先重置所有状态
    chunks.clear();
    m_clientMap.clear();
    totalChunks = 0;
    receivedCount = 0;
    failed = false;
    tempDir.clear();
    outputFilePath.clear();

    // 1. 读取文本文件（强制UTF-8解码，避免乱码）
    QFile file(inputFile);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Cannot open input file");
        return;
    }
    QByteArray rawData = file.readAll();
    QString fullText = QString::fromUtf8(rawData);
    file.close();

    QStringList lines = fullText.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (!line.isEmpty() && !QString("。！？；：.").contains(line.back())) {
            lines[i] = lines[i] + "。";
        }
    }
    fullText = lines.join("\n");


    // 2. 文本分片
    chunks = TextSplitter::split(fullText);
    if (chunks.isEmpty()) {
        emit errorOccurred("No text to synthesize");
        return;
    }

    // 3. 创建临时目录
    tempDir = QDir(outputDir).filePath("temp_chunks");//返回Qstring
    QDir dir(tempDir);
    if (dir.exists()) {
        dir.removeRecursively();//递归删除文件夹中所有文件和他自己
    }
    if (!dir.mkpath(tempDir)) {//创建目录，如果不成功，报错
        emit errorOccurred("Cannot create temp directory");
        return;
    }

    // 最终输出文件路径
    outputFilePath = QDir(outputDir).filePath("output.wav");
    totalChunks = chunks.size();

    //4. 批量发起所有分片合成请求
    sendAllChunks(speaker_id, speed);
}

void TtsManager::sendAllChunks(int speaker_id, double speed)
{
    // 缓存参数，给 sendNextChunks 用
    m_speakerId = speaker_id;
    m_speed = speed;

    // 所有分片索引入队
    m_pendingChunks.clear();
    for (int i = 0; i < totalChunks; ++i) {
        m_pendingChunks.enqueue(i);
    }
    m_activeCount = 0;

    // 启动发送
    sendNextChunks();
}

void TtsManager::sendNextChunks()
{
    // 只在未满并发 且 队列非空时发送
    while (m_activeCount < kMaxConcurrent && !m_pendingChunks.isEmpty()) {
        if (failed) return;

        int i = m_pendingChunks.dequeue();
        m_activeCount++;

        TtsClient* client = new TtsClient(m_baseUrl, this);
        m_clientMap[client] = i;

        // ========== 成功回调 ==========
        connect(client, &TtsClient::success, this,
                [this, client, i](const QByteArray& audioBase64)
                {
                    m_activeCount--;

                    if (failed) {
                        client->deleteLater();
                        m_clientMap.remove(client);
                        return;
                    }

                    QByteArray audioData = QByteArray::fromBase64(audioBase64);
                    QString tempPath = getTempFilePath(i);
                    QFile tempFile(tempPath);
                    if (!tempFile.open(QIODevice::WriteOnly)) {
                        failed = true;
                        emit errorOccurred(QString("Cannot write temp file for chunk %1").arg(i));
                        client->deleteLater();
                        m_clientMap.remove(client);
                        cleanup();
                        return;
                    }
                    tempFile.write(audioData);
                    tempFile.close();

                    receivedCount++;
                    emit progressUpdated(receivedCount, totalChunks);

                    client->deleteLater();
                    m_clientMap.remove(client);

                    //完成后补位
                    if (receivedCount == totalChunks) {
                        mergeAll();
                    } else {
                        sendNextChunks();
                    }
                });

        // ========== 失败回调 ==========
        connect(client, &TtsClient::error, this,
                [this, client, i](const QString& msg)
                {
                    m_activeCount--;

                    if (failed) {
                        client->deleteLater();
                        m_clientMap.remove(client);
                        return;
                    }

                    failed = true;
                    emit errorOccurred(QString("Chunk %1 error: %2").arg(i).arg(msg));

                    // 清空队列，不再发新请求
                    m_pendingChunks.clear();

                    client->deleteLater();
                    m_clientMap.remove(client);
                    cleanup();
                });

        // 发起请求
        client->synthesize(chunks[i], m_speakerId, m_speed);
    }
}

void TtsManager::mergeAll()
{
    if (failed) {
        cleanup();
        return;
    }

    // 1. 收集所有分片路径
    QStringList chunkFiles;
    for (int i = 0; i < totalChunks; ++i) {
        chunkFiles << getTempFilePath(i);
    }

    // 2. 调用新函数，传入 2GB 作为阈值 (2LL * 1024 * 1024 * 1024),注意，单位是byte，字节
    QStringList outputFiles = WavUtils::mergeWavFilesSplit(
        chunkFiles, outputFilePath, 2LL * 1024 * 1024 * 1024);

    // 3. 如果什么都没生成，报错
    if (outputFiles.isEmpty()) {
        emit errorOccurred("Failed to merge audio files");
        cleanup();
        return;
    }

    // 4. 发送完成信号
    // outputFiles 可能只有 1 个文件，也可能有多个文件（如果你合成的东西超大）
    // 我们把多个路径用换行符 "\n" 拼起来，直接传给 UI
    emit synthesisFinished(outputFiles.join("\n"));

    cleanup();
}

QString TtsManager::getTempFilePath(int index) const
{
    // 生成4位补零的分片文件名，例如 chunk_0000.wav
    return QDir(tempDir).filePath(
        QString("chunk_%1.wav").arg(index, 6, 10, QChar('0'))//%1，占位符,后面的arg参数含义，填到占位符的数字，不满6位补第四个参数传的东西，10进制
        );
}

void TtsManager::cleanup()
{
    // 递归删除整个临时目录，不留残留
    QDir dir(tempDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
}

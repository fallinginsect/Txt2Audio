#include "TtsManager.h"
#include "TextSplitter.h"
#include "WavUtils.h"
#include <QFile>
#include <QDir>
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
    for (int i = 0; i < totalChunks; ++i) {
        // 每个分片创建独立的TtsClient，避免并发请求互相干扰
        TtsClient* client = new TtsClient(m_baseUrl, this);//传TTSmanager作为父对象，如果manager析构了，会先释放所有的client内存
        m_clientMap[client] = i;//当前分片序号

        // 连接成功信号：lambda捕获分片序号，解决并发对应问题
        connect(client, &TtsClient::success, this,
                [this, client, i](const QByteArray& audioBase64)
                {
                    if (failed) {//前面有个分片出问题了，后面的作废，初版本先这样
                        client->deleteLater();//后面我们再想什么重传啊等等
                        m_clientMap.remove(client);
                        return;
                    }

                    // Base64解码得到原始WAV二进制
                    QByteArray audioData = QByteArray::fromBase64(audioBase64);

                    // 保存为分片临时文件
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

                    // 清理当前客户端
                    client->deleteLater();
                    m_clientMap.remove(client);

                    // 所有分片接收完成，执行合并
                    if (receivedCount == totalChunks) {
                        mergeAll();
                    }
                });

        // 连接错误信号
        connect(client, &TtsClient::error, this,
                [this, client, i](const QString& msg)
                {
                    if (failed) {
                        client->deleteLater();
                        m_clientMap.remove(client);
                        return;
                    }

                    failed = true;
                    emit errorOccurred(QString("Chunk %1 error: %2").arg(i).arg(msg));

                    client->deleteLater();
                    m_clientMap.remove(client);
                    cleanup();
                });

        // 发起当前分片的合成请求
        client->synthesize(chunks[i], speaker_id, speed);
    }
}

void TtsManager::mergeAll()
{
    if (failed) {
        cleanup();//发生错误就清除
        return;
    }

    // 按索引顺序收集所有分片文件（保证顺序正确，和发送顺序无关）
    QStringList chunkFiles;
    for (int i = 0; i < totalChunks; ++i) {
        chunkFiles << getTempFilePath(i);
    }

    // 调用工具类合并WAV
    if (!WavUtils::mergeWavFiles(chunkFiles, outputFilePath)) {
        emit errorOccurred("Failed to merge audio files");
        cleanup();
        return;
    }

    emit synthesisFinished(outputFilePath);
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

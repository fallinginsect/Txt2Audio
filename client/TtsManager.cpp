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
    m_client = new TtsClient(baseUrl, this);
    connect(m_client, &TtsClient::success, this, &TtsManager::onChunkSuccess);
    connect(m_client, &TtsClient::error, this, &TtsManager::onChunkError);
}

void TtsManager::cancelTask(const QString& reason)
{
    if (failed) return; // 已经停了就不重复停了
    failed = true;

    qDebug() << "取消任务:" << reason;

    m_pendingChunks.clear();

    // 通知服务端
    QNetworkAccessManager* mgr = new QNetworkAccessManager(this);
    QUrl url(m_baseUrl + "/cancel");
    mgr->post(QNetworkRequest(url), QByteArray());

    m_client->abortAll();


    emit errorOccurred(reason);
    cleanup();
}


void TtsManager::synthesizeFile(const QString& inputFile, const QString& outputDir,
                                int speaker_id, double speed)
{
    // 重置服务端状态
    QNetworkAccessManager* mgr = new QNetworkAccessManager(this);
    QUrl url(m_baseUrl + "/reset");
    mgr->post(QNetworkRequest(url), QByteArray());

    //  每次新任务先重置所有状态
    chunks.clear();
    totalChunks = 0;
    receivedCount = 0;
    failed = false;
    tempDir.clear();
    outputFilePath.clear();
    m_retryCounts.clear();

    // 1. 读取文本文件（强制UTF-8解码，避免乱码）
    QFile file(inputFile);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Cannot open input file");
        return;
    }
    QByteArray rawData = file.readAll();
    QString fullText = QString::fromUtf8(rawData);
    file.close();

    //现预处理，如果有换行但是结尾没有标点的地方我们加上标点来让输出的停顿合理
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
    while (m_activeCount < kMaxConcurrent && !m_pendingChunks.isEmpty()) {
        if (failed) return;

        int i = m_pendingChunks.dequeue();
        m_activeCount++;

        //
        m_client->synthesize(i, chunks[i], m_speakerId, m_speed);
    }
}

void TtsManager::onChunkSuccess(int chunkIndex, const QByteArray& audioBase64)
{
    m_activeCount--; // 在飞请求数减一

    // 如果整个任务已经被取消或彻底失败，直接丢弃这个迟到的数据
    if (failed) return;

    // 1. 解码并保存为分片临时文件
    QByteArray audioData = QByteArray::fromBase64(audioBase64);
    QString tempPath = getTempFilePath(chunkIndex);
    QFile tempFile(tempPath);

    if (!tempFile.open(QIODevice::WriteOnly)) {
        // 如果写文件失败，属于严重的本地IO错误，直接取消整个任务
        cancelTask(QString("无法写入临时文件，分片：%1").arg(chunkIndex));
        return;
    }

    tempFile.write(audioData);
    tempFile.close();

    // 2. 更新进度
    receivedCount++;
    emit progressUpdated(receivedCount, totalChunks);

    // 3. 检查是否全部完成
    if (receivedCount == totalChunks) {
        mergeAll(); // 所有分片到齐，开始合并
    } else {
        sendNextChunks(); // 还没完，继续从队列里取下一个补位
    }
}

void TtsManager::onChunkError(int chunkIndex, const QString& msg)
{
    m_activeCount--; // 在飞请求数减一

    // 如果任务已经被取消了，忽略这个错误
    if (failed) return;

    // 1. 记录该分片的失败次数
    m_retryCounts[chunkIndex]++;

    // 2. 如果失败次数小于 3，重新塞回队列头部进行重试
    if (m_retryCounts[chunkIndex] < 3) {
        qDebug() << "分片" << chunkIndex << "失败，正在重试第" << m_retryCounts[chunkIndex] << "次。原因:" << msg;
        m_pendingChunks.prepend(chunkIndex);
        sendNextChunks();
        return;
    }

    // 3. 超过 3 次，彻底放弃，直接取消整个任务
    cancelTask(QString("分片 %1 多次重试失败: %2").arg(chunkIndex).arg(msg));
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

    // 2. 调用函数，传入 2GB 作为阈值 (2LL * 1024 * 1024 * 1024),注意，单位是byte，字节
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

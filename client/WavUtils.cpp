#include "WavUtils.h"
#include <QDebug>
#include <QFileInfo>
#include <Qdir>

bool WavUtils::readWavFile(const QString& filePath, QByteArray& pcmData, WavHeader& header) {
    QFile file(filePath);//对应音频文件的路径
    if (!file.open(QIODevice::ReadOnly)) return false;//打开失败

    if (file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader)) != sizeof(WavHeader)) {
        file.close();//read()只接收char*,这边看一看看是不是44字节的标准wav头，
        return false;
    }

    // 检查是否为标准 WAV
    if (memcmp(header.chunkID, "RIFF", 4) != 0 || memcmp(header.format, "WAVE", 4) != 0) {
        file.close();
        return false;
    }
    // 跳转到数据块
    // MeloTTS 输出是标准 WAV
    // 这里假定偏移量固定为 44 字节
    file.seek(44);
    pcmData = file.readAll();
    file.close();
    return true;
}

bool WavUtils::writeWavFile(const QString& filePath, const QByteArray& pcmData, const WavHeader& header) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;//截断之前文件里的内容
    // 写入头部
    WavHeader h = header;
    h.subchunk2Size = pcmData.size();
    h.chunkSize = 36 + pcmData.size(); // 36 = 44 - 8，注意如果数据部分过大，这里会有偏差，我们这里先不处理
    if (file.write(reinterpret_cast<const char*>(&h), sizeof(WavHeader)) != sizeof(WavHeader)) {//比较一下h是否符合标准
        file.close();
        return false;
    }
    if (file.write(pcmData) != pcmData.size()) {//写入Data失败
        file.close();
        return false;
    }
    file.close();
    return true;
}

static bool mergeWavFiles(const QStringList& inputFiles, const QString& outputFile) {

    using WavUtils::readWavFile;
    using WavUtils::writeWavFile;


    if (inputFiles.isEmpty()) return false;
    QByteArray mergedPcm;
    WavHeader firstHeader;
    bool headerRead = false;
    for (const QString& filePath : inputFiles) {//遍历QstringList
        QByteArray pcm;
        WavHeader header;
        if (!readWavFile(filePath, pcm, header)) {
            qWarning() << "Failed to read" << filePath;
            return false;
        }
        if (!headerRead) {//如果是第一个文件
            firstHeader = header;
            headerRead = true;
        } else {
            // 检查参数是否一致
            if (header.sampleRate != firstHeader.sampleRate ||
                header.numChannels != firstHeader.numChannels ||
                header.bitsPerSample != firstHeader.bitsPerSample) {//参数不一样就不合并了
                qWarning() << "Inconsistent audio parameters";
                return false;
            }
        }
        mergedPcm.append(pcm);
    }
    if (!headerRead) return false;
    // 写入合并后的文件
    return writeWavFile(outputFile, mergedPcm, firstHeader);
}

QStringList WavUtils::mergeWavFilesSplit(const QStringList& inputFiles,
                                         const QString& outputBasePath,
                                         qint64 maxBytesPerFile) {
    QStringList outputFiles;
    if (inputFiles.isEmpty()) return outputFiles;

    QByteArray currentPcm;      // 攒数据
    WavHeader header;
    bool headerRead = false;
    int partIndex = 1;          // 记录当前是第几个文件

    // 从目标路径里拆出文件夹和文件名（比如 output.wav 拆成 "目录" 和 "output"）
    QString dir = QFileInfo(outputBasePath).absolutePath();
    QString baseName = QFileInfo(outputBasePath).completeBaseName();

    // 这是一个辅助小函数（Lambda），用来把箱子里的数据写到硬盘
    auto flushPart = [&]() -> bool {
        if (currentPcm.isEmpty() || !headerRead) return false;

        // 生成文件名，比如 output_part1.wav
        QString partPath = QDir(dir).filePath(
            QString("%1_part%2.wav").arg(baseName).arg(partIndex++)
            );

        if (!writeWavFile(partPath, currentPcm, header)) return false;

        outputFiles << partPath; // 把新生成的文件路径记下来
        currentPcm.clear();      // 清空箱子，准备装下一批
        return true;
    };

    // 开始遍历所有的小分片
    for (const QString& filePath : inputFiles) {
        QByteArray pcm;
        WavHeader h;
        if (!readWavFile(filePath, pcm, h)) return outputFiles;

        // 如果是第一个分片，记下它的音频格式（采样率等）
        if (!headerRead) {
            header = h;
            headerRead = true;
        } else if (h.sampleRate != header.sampleRate) {
            // 格式不一致就不合并了，直接返回,虽然不太可能出现这种情况，目前的话
            return outputFiles;
        }

        //核心逻辑：如果加上这一片会超过 2GB，且箱子里已经有东西了
        if (!currentPcm.isEmpty() && (currentPcm.size() + pcm.size() > maxBytesPerFile)) {
            if (!flushPart()) return outputFiles; // 把当前的写出去，清空箱子
        }

        // 把这一片装进箱子
        currentPcm.append(pcm);
    }

    // 循环结束后，箱子里可能还有剩下的最后一点，写出去
    flushPart();

    return outputFiles;
}
#include "WavUtils.h"
#include <QDebug>

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

bool WavUtils::mergeWavFiles(const QStringList& inputFiles, const QString& outputFile) {
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
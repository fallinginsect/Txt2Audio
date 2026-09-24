#ifndef WAVUTILS_H
#define WAVUTILS_H

#include <QByteArray>
#include <QFile>


#pragma pack(1)//防止struct对齐

//这里说一下标准wav头的格式，RIFF(12字节) + fmt格式子块(24字节) + data数据块头(8字节)，文件头之后就是PCM音频二进制数据
//注意PCM音频数据是一段连续排列的数字列表，数字是对声音某种形式解析出来的，有一些格式，比如线性PCM,无损无压缩
//比如某些压缩语音编码，这时候文件头不一定44字节，因为还要带上编码表，压缩系数的信息等
//采样是每个一段时间取一个声音数值，采样的点越密集，声音细节还原越准确
//采样位深，音频的数值用多少位2进制表示，一般越高，层次越丰富

struct WavHeader {
    char chunkID[4];      // 固定为"RIFF"，表示是RIFF容器格式文件
    uint32_t chunkSize;   //这个内容之后，剩下的文件的所有字节数，也就是除去chunkID,chunkSize,剩下的东西的字节数，总体来说是文件字节数 - 8
    char format[4];       // 固定为"WAVE"，表示这是WAVE音频文件
    char subchunk1ID[4];  // 固定为"fmt "，注意后面的空格是必要的
    uint32_t subchunk1Size;//fmt板块后续内容的字节数，线性PCM固定式16
    uint16_t audioFormat;//音频编码格式，1表示我们这里的线性PCM
    uint16_t numChannels;//声道数，比如1表示单声道
    uint32_t sampleRate;//采样率，HZ,应该是采样再一秒钟内发生的次数，melotts这里是22050或者44100
    uint32_t byteRate;//每秒音频占用的字节数，播放器用来计算播放缓冲区的大小,,simpleRate*numChannels*bitPerSample/8
    uint16_t blockAlign;//单个采样点占用的字节数(所有声道合计),numChannels*bitsPersample/8
    uint16_t bitsPerSample;//采样位深
    char subchunk2ID[4];  // 固定值"data"
    uint32_t subchunk2Size;//音频数据字节数，这里是32位无符号数，所以最大能表示到2^32 - 1，大概4GB,所以线性PCM格式wav音频文件最大4GB左右
};
#pragma pack()//恢复默认设置


namespace WavUtils {
bool readWavFile(const QString& filePath, QByteArray& pcmData, WavHeader& header);
bool writeWavFile(const QString& filePath, const QByteArray& pcmData, const WavHeader& header);
bool mergeWavFiles(const QStringList& inputFiles, const QString& outputFile);
}

#endif // WAVUTILS_H

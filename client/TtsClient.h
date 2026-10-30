#ifndef TTSCLIENT_H
#define TTSCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>

class TtsClient : public QObject {
    Q_OBJECT
public:
    explicit TtsClient(const QString& baseUrl = "http://127.0.0.1:8000", QObject* parent = nullptr);//包含NetowrkManager
    void synthesize(int chunkIndex, const QString& text, int speaker_id = 1, double speed = 1.0);
    void abortAll(); //切断网络
signals://信号
    void success(int chunkIndex, const QByteArray& audioBase64);
    void error(int chunkIndex, const QString& msg);
private slots://槽函数
    void onReplyFinished(QNetworkReply* reply);
private:
    QNetworkAccessManager* manager;
    QString baseUrl;
    QMap<QNetworkReply*, int> m_replyMap; //存下当前的请求指针和对应序号
};

#endif // TTSCLIENT_H

#ifndef TTSCLIENT_H
#define TTSCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class TtsClient : public QObject {
    Q_OBJECT
public:
    explicit TtsClient(const QString& baseUrl = "http://127.0.0.1:8000", QObject* parent = nullptr);//包含NetowrkManager
    void synthesize(const QString& text, int speaker_id = 1, double speed = 1.0);
signals://信号
    void success(const QByteArray& audioBase64);
    void error(const QString& msg);
private slots://槽函数
    void onReplyFinished(QNetworkReply* reply);
private:
    QNetworkAccessManager* manager;
    QString baseUrl;
};

#endif // TTSCLIENT_H

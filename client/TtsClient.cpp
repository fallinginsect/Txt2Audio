#include "TtsClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

TtsClient::TtsClient(const QString& baseUrl, QObject* parent)
    : QObject(parent), baseUrl(baseUrl) {
    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &TtsClient::onReplyFinished);
}

void TtsClient::synthesize(const QString& text, int speaker_id, double speed) {
    QUrl url(baseUrl + "/synthesize");
    QNetworkRequest request(url);//HTTP所有的信息都打包在这个对象里，地址，请求方法等等
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");//设置请求头和表示这次请求体是json模式

    QJsonObject obj;
    obj["text"] = text;
    obj["speaker_id"] = speaker_id;
    obj["speed"] = speed;
    QJsonDocument doc(obj);
    QByteArray body = doc.toJson();

    manager->post(request, body);
}

void TtsClient::onReplyFinished(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());//错误信息
        reply->deleteLater();//reply对应的所有事件处理完之后再删除
        return;
    }
    QByteArray data = reply->readAll();//拿传输来的数据
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {//空的或者不是键值对对象
        emit error("Invalid JSON response");
        reply->deleteLater();
        return;
    }
    QJsonObject obj = doc.object();
    if (!obj.contains("audio")) {//没有音频信息
        emit error("Missing 'audio' field in response");
        reply->deleteLater();
        return;
    }
    QString audioBase64 = obj["audio"].toString();//QJsonValue->Qstring
    emit success(audioBase64.toUtf8());//发送信号，传数据过去
    reply->deleteLater();
}
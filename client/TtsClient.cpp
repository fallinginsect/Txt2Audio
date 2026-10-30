#include "TtsClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

TtsClient::TtsClient(const QString& baseUrl, QObject* parent)
    : QObject(parent), baseUrl(baseUrl) {
    manager = new QNetworkAccessManager(this);
}

void TtsClient::synthesize(int chunkIndex, const QString& text, int speaker_id, double speed) {
    QUrl url(baseUrl + "/synthesize");
    QNetworkRequest request(url);//HTTP所有的信息都打包在这个对象里，地址，请求方法等等
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");//设置请求头和表示这次请求体是json模式

    QJsonObject obj;
    obj["text"] = text;
    obj["speaker_id"] = speaker_id;
    obj["speed"] = speed;
    QJsonDocument doc(obj);
    QByteArray body = doc.toJson();

    QNetworkReply* reply = manager->post(request, body);

    // 记录对应关系
    m_replyMap[reply] = chunkIndex;

    // 绑定这个具体的 reply
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
    });
}

void TtsClient::abortAll() {//注意，这里是已经发出去的请求，基本上还存在队列里的都是在跑的
    // 1. 先取出所有的 key，拷贝一份。这样即使循环里面删了 Map，也不会报错
    QList<QNetworkReply*> replies = m_replyMap.keys();

    // 2. 只负责掐断网线，引发 finished 信号
    for (auto* reply : replies) {
        if (reply->isRunning()) {
            reply->abort(); //这里abort()之后立马就会触发finished信号，然后进入下面这个函数来删除Map里面的记录
        }
    }
}

void TtsClient::onReplyFinished(QNetworkReply* reply) {
    int chunkIndex = m_replyMap.value(reply, -1);//能找到对应id就返回，否则返回我们填的-1
    m_replyMap.remove(reply); // 处理完就移除
    reply->deleteLater();//注意这里是不会和abortall冲突的

    if (chunkIndex == -1) return; // 安全检查

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        return; // 被主动取消的，安静退出
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit error(chunkIndex, reply->errorString());
        return;
    }
    QByteArray data = reply->readAll();//拿传输来的数据
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {//空的或者不是键值对对象
        emit error(chunkIndex, "Invalid JSON response");
        reply->deleteLater();
        return;
    }
    QJsonObject obj = doc.object();
    if (!obj.contains("audio")) {//没有音频信息
        emit error(chunkIndex, "Missing 'audio' field in response");
        reply->deleteLater();
        return;
    }
    QString audioBase64 = obj["audio"].toString();//QJsonValue->Qstring
    emit success(chunkIndex, audioBase64.toUtf8());
    reply->deleteLater();
}
#include "mainwindow.h"
#include "DropArea.h"
#include "TtsManager.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QProgressBar>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringDecoder>
#include <QSizePolicy>
#include <QDoubleValidator>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_generateSuccess(false)
    , m_ttsManager(new TtsManager(this))
    , m_netMgr(new QNetworkAccessManager(this))

{
    setWindowTitle("TXT转音频工具 v0.1");
    setMinimumSize(640, 520);

    setupUi();
    connectSignals();
    loadVoiceOptions();
}

MainWindow::~MainWindow()
{
}


void MainWindow::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // 1. 文件拖放区域
    m_dropArea = new DropArea(centralWidget);
    m_dropArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(m_dropArea);

    // 2. 文件名标签
    m_fileNameLabel = new QLabel("请拖入TXT文件，或点击区域选择文件", centralWidget);
    m_fileNameLabel->setAlignment(Qt::AlignCenter);
    m_fileNameLabel->setStyleSheet(R"(
        QLabel {
            color: #E5E7EB;
            font-size: 15px;
            font-weight: 500;
        }
    )");
    mainLayout->addWidget(m_fileNameLabel);

    // 3. 确认加载按钮
    m_confirmBtn = new QPushButton("确认加载文件", centralWidget);
    m_confirmBtn->setEnabled(false);
    m_confirmBtn->setMinimumHeight(32);
    m_confirmBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mainLayout->addWidget(m_confirmBtn);

    // 分隔线 - 适配深色背景
    QFrame* line1 = new QFrame(centralWidget);
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    line1->setStyleSheet("color: #374151;");
    mainLayout->addWidget(line1);

    // 4. 输出目录行
    QHBoxLayout* outputLayout = new QHBoxLayout();
    outputLayout->setSpacing(10);

    m_outputPathEdit = new QLineEdit(centralWidget);
    m_outputPathEdit->setPlaceholderText("请选择输出文件夹");
    m_outputPathEdit->setReadOnly(true);
    m_outputPathEdit->setMinimumHeight(30);
    outputLayout->addWidget(m_outputPathEdit, 1);

    m_browseBtn = new QPushButton("浏览", centralWidget);
    m_browseBtn->setMinimumWidth(80);
    m_browseBtn->setMinimumHeight(30);
    outputLayout->addWidget(m_browseBtn);

    mainLayout->addLayout(outputLayout);

    // 5. 参数选择行：添加标签说明
    QHBoxLayout* paramLayout = new QHBoxLayout();
    paramLayout->setSpacing(10);

    // 音色标签
    QLabel* voiceLabel = new QLabel("音色：", centralWidget);
    voiceLabel->setStyleSheet("color: #E5E7EB; font-size: 14px;");
    voiceLabel->setMinimumWidth(50);
    voiceLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    paramLayout->addWidget(voiceLabel);

    // 音色选择下拉框
    m_voiceCombo = new QComboBox(centralWidget);
    m_voiceCombo->setMinimumHeight(30);
    paramLayout->addWidget(m_voiceCombo, 1);

    // 语速标签
    QLabel* speedLabel = new QLabel("语速：", centralWidget);
    speedLabel->setStyleSheet("color: #E5E7EB; font-size: 14px;");
    speedLabel->setMinimumWidth(50);
    speedLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    paramLayout->addWidget(speedLabel);

    // 语速输入框
    m_speedEdit = new QLineEdit(centralWidget);
    m_speedEdit->setPlaceholderText("语速");
    m_speedEdit->setText("1.0");
    m_speedEdit->setMinimumHeight(30);
    m_speedEdit->setAlignment(Qt::AlignCenter);
    QDoubleValidator* speedValidator = new QDoubleValidator(0.5, 2.0, 2, m_speedEdit);
    speedValidator->setNotation(QDoubleValidator::StandardNotation);
    m_speedEdit->setValidator(speedValidator);
    paramLayout->addWidget(m_speedEdit, 1);

    mainLayout->addLayout(paramLayout);

    // 6. 开始合成按钮
    m_startBtn = new QPushButton("开始合成", centralWidget);
    m_startBtn->setEnabled(false);
    m_startBtn->setMinimumHeight(40);
    m_startBtn->setStyleSheet(R"(
        QPushButton {
            font-size: 15px;
            font-weight: bold;
            background-color: #3B82F6;
            color: white;
            border-radius: 6px;
        }
        QPushButton:disabled {
            background-color: #1E40AF;
            color: #9CA3AF;
        }
        QPushButton:hover {
            background-color: #2563EB;
        }
    )");
    mainLayout->addWidget(m_startBtn);

    // 7. 进度条
    m_progressBar = new QProgressBar(centralWidget);
    m_progressBar->setVisible(false);
    m_progressBar->setMinimumHeight(8);
    m_progressBar->setTextVisible(false);
    mainLayout->addWidget(m_progressBar);

    // 8. 底部状态提示
    m_statusLabel = new QLabel("就绪", centralWidget);
    m_statusLabel->setAlignment(Qt::AlignLeft);
    m_statusLabel->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    mainLayout->addWidget(m_statusLabel);
}


// 统一连接信号槽
void MainWindow::connectSignals()
{
    connect(m_dropArea, &DropArea::fileSelected, this, [=](const QString &path) {
        m_currentFilePath = path;
        m_fileNameLabel->setText(QFileInfo(path).fileName());
        m_confirmBtn->setEnabled(true);
        m_statusLabel->setText("已选择文件，点击确认加载");
        m_generateSuccess = false;
    });

    connect(m_confirmBtn, &QPushButton::clicked, this, &MainWindow::readTxtFile);
    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseBtnClicked);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartBtnClicked);

    connect(m_ttsManager, &TtsManager::progressUpdated, this, &MainWindow::onSynthesisProgress);
    connect(m_ttsManager, &TtsManager::synthesisFinished, this, &MainWindow::onSynthesisFinished);
    connect(m_ttsManager, &TtsManager::errorOccurred, this, &MainWindow::onSynthesisError);
}

// ==============================
// 槽函数：读取文本文件
// ==============================
void MainWindow::readTxtFile()
{
    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法打开文件：" + m_currentFilePath);
        return;
    }

    QByteArray raw = file.readAll();
    file.close();

    QStringDecoder utf8Decoder("UTF-8");
    QString text = utf8Decoder(raw);
    if (utf8Decoder.hasError()) {
        QStringDecoder gbkDecoder("GBK");
        text = gbkDecoder(raw);
    }

    m_txtContent = text;
    m_statusLabel->setText("文件加载成功，共 " + QString::number(text.length()) + " 字");

    if (!m_outputDir.isEmpty()) {
        m_startBtn->setEnabled(true);
    }
}

// ==============================
// 槽函数：浏览输出目录
// ==============================
void MainWindow::onBrowseBtnClicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "选择输出文件夹",
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

    if (!dir.isEmpty()) {
        m_outputDir = dir;
        m_outputPathEdit->setText(dir);

        if (!m_txtContent.isEmpty()) {
            m_startBtn->setEnabled(true);
        }
    }
}

// ==============================
// 槽函数：开始合成
// ==============================
void MainWindow::onStartBtnClicked()
{
    if (m_currentFilePath.isEmpty() || m_outputDir.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择文本文件和输出目录");
        return;
    }

    int speakerId = m_voiceCombo->currentData().toInt();

    // 语速二次校验
    bool ok;
    double speed = m_speedEdit->text().toDouble(&ok);
    if (!ok || speed < 0.5 || speed > 2.0) {
        speed = 1.0;
        m_speedEdit->setText("1.0");
        QMessageBox::information(this, "提示", "语速输入不合法，已自动重置为1.0\n支持范围：0.5 ~ 2.0");
    }

    m_startBtn->setEnabled(false);
    m_browseBtn->setEnabled(false);
    m_confirmBtn->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText("正在合成...");

    m_ttsManager->synthesizeFile(m_currentFilePath, m_outputDir, speakerId, speed);
}

// ==============================
// 槽函数：加载音色列表
// ==============================
void MainWindow::loadVoiceOptions()
{
    QUrl url("http://127.0.0.1:8000/speakers");
    QNetworkRequest request(url);
    QNetworkReply* reply = m_netMgr->get(request);
    connect(reply, &QNetworkReply::finished, this, &MainWindow::onSpeakersLoaded);
}

void MainWindow::onSpeakersLoaded()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        m_statusLabel->setText("警告：音色列表加载失败，请检查服务端");
        m_voiceCombo->addItem("默认音色", 1);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonArray speakers = doc.object()["speakers"].toArray();

    for (const QJsonValue& val : speakers) {
        QJsonObject spk = val.toObject();
        m_voiceCombo->addItem(spk["name"].toString(), spk["id"].toInt());
    }

    m_statusLabel->setText("音色列表加载完成");
    reply->deleteLater();
}

// ==============================
// 槽函数：合成进度更新
// ==============================
void MainWindow::onSynthesisProgress(int current, int total)
{
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
    m_statusLabel->setText(QString("合成中：%1 / %2 分片").arg(current).arg(total));
}

// ==============================
// 槽函数：合成成功
// ==============================
void MainWindow::onSynthesisFinished(const QString& outputFile)
{
    m_generateSuccess = true;
    m_statusLabel->setText("合成完成！文件已保存");
    m_progressBar->setVisible(false);

    m_startBtn->setEnabled(true);
    m_browseBtn->setEnabled(true);
    m_confirmBtn->setEnabled(true);

    QMessageBox::information(this, "合成成功", "音频文件已保存至：\n" + outputFile);
}

// ==============================
// 槽函数：合成失败
// ==============================
void MainWindow::onSynthesisError(const QString& msg)
{
    m_generateSuccess = false;
    m_statusLabel->setText("合成失败");
    m_progressBar->setVisible(false);

    m_startBtn->setEnabled(true);
    m_browseBtn->setEnabled(true);
    m_confirmBtn->setEnabled(true);

    QMessageBox::critical(this, "合成失败", "错误信息：\n" + msg);
}

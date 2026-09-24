#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>

// 前置声明，减少头文件依赖
class DropArea;
class QLabel;
class QPushButton;
class QLineEdit;
class QComboBox;
class QProgressBar;
class TtsManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void readTxtFile();
    void onBrowseBtnClicked();
    void onStartBtnClicked();
    void loadVoiceOptions();

    void onSynthesisProgress(int current, int total);
    void onSynthesisFinished(const QString& outputFile);
    void onSynthesisError(const QString& msg);
    void onSpeakersLoaded();

private:
    // 界面控件
    DropArea* m_dropArea;
    QLabel* m_fileNameLabel;
    QPushButton* m_confirmBtn;
    QLineEdit* m_outputPathEdit;
    QPushButton* m_browseBtn;
    QComboBox* m_voiceCombo;
    QLineEdit* m_speedEdit;   // 语速改为输入框
    QPushButton* m_startBtn;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    // 业务状态
    QString m_currentFilePath;
    QString m_txtContent;
    QString m_outputDir;
    bool m_generateSuccess;
    QString m_errorMsg;

    TtsManager* m_ttsManager;
    QNetworkAccessManager* m_netMgr;

    void setupUi();
    void connectSignals();
};

#endif // MAINWINDOW_H


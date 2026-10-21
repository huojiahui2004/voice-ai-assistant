#pragma once

#include <QMainWindow>
#include <QList>
#include <QPair>
#include <QString>

class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;

class ConfigManager;
class AudioRecorder;
class VoskRecognizer;
class LlmClient;
class PiperTts;
class AudioPlayer;

// 主窗体:Qt Widgets UI + 信号槽编排,把
// 录音 -> Vosk -> 在线 LLM(DeepSeek/Groq) -> Piper -> 播放 串成闭环,并提供依赖状态面板与优雅降级。
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onTalkPressed();
    void onTalkReleased();
    void onSendText();
    void onClearChat();
    void onOpenConfigFolder();

private:
    void setupUi();
    void setupPipeline();
    void startAssistantTurn();
    void appendUserMessage(const QString &text);
    void appendAssistantChunk(const QString &delta);
    void renderChat();
    void setStatus(const QString &text);
    void refreshDependencyPanel();
    void updateControlsEnabled();

    ConfigManager  *m_cfg   = nullptr;
    AudioRecorder  *m_rec    = nullptr;
    VoskRecognizer *m_vosk   = nullptr;
    LlmClient      *m_llm    = nullptr;
    PiperTts       *m_piper  = nullptr;
    AudioPlayer    *m_player = nullptr;

    // UI
    QTextEdit   *m_chat     = nullptr;
    QLineEdit   *m_input    = nullptr;
    QPushButton *m_talkBtn  = nullptr;
    QPushButton *m_clearBtn= nullptr;
    QPushButton *m_openCfgBtn = nullptr;
    QLabel      *m_status   = nullptr;
    QLabel      *m_depLabel = nullptr;

    // 对话状态
    using Msg = QPair<QString, QString>;  // role, content
    QList<Msg> m_history;
    QString m_assistantBuf;   // 当前助手消息流式累积
    bool m_pipelineBusy = false;

    // LLM 连通性探活结果
    bool m_llmOnline = false;
    QString m_llmInfo;
};

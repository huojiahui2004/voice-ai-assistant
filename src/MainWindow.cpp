#include "MainWindow.h"

#include "ConfigManager.h"
#include "AudioRecorder.h"
#include "VoskRecognizer.h"
#include "LlmClient.h"
#include "PiperTts.h"
#include "AudioPlayer.h"

#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTextCursor>
#include <QScrollBar>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QFrame>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_cfg = new ConfigManager(this);
    setupUi();
    setupPipeline();
}

// ---------------- UI ----------------

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("小夏 - 你的女朋友 💕"));
    resize(980, 700);

    auto *central = new QWidget(this);
    setCentralWidget(central);

    m_chat = new QTextEdit(central);
    m_chat->setReadOnly(true);
    m_chat->setPlaceholderText(QStringLiteral("对话记录将显示在这里…\n按住下方“🎤 按住说话”开始,或在输入框敲字回车发送。"));
    m_chat->setMinimumHeight(320);

    m_input = new QLineEdit(central);
    m_input->setPlaceholderText(QStringLiteral("输入文字,回车发送(无 ASR 时也可用)…"));

    auto *sendBtn = new QPushButton(QStringLiteral("发送"), central);
    m_clearBtn = new QPushButton(QStringLiteral("清空"), central);

    m_talkBtn = new QPushButton(QStringLiteral("🎤 按住说话"), central);
    m_talkBtn->setMinimumHeight(52);
    m_talkBtn->setCursor(Qt::PointingHandCursor);

    m_status = new QLabel(QStringLiteral("就绪"), central);
    m_status->setStyleSheet(QStringLiteral("color:#374151; padding:4px;"));

    // 右侧依赖面板
    m_depLabel = new QLabel(central);
    m_depLabel->setWordWrap(true);
    m_depLabel->setTextFormat(Qt::RichText);
    m_depLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_openCfgBtn = new QPushButton(QStringLiteral("打开资源/配置文件夹"), central);

    auto *depTitle = new QLabel(QStringLiteral("<b>依赖状态</b>"), central);
    auto *right = new QVBoxLayout();
    right->setSpacing(8);
    right->addWidget(depTitle);
    right->addWidget(m_depLabel, 1);
    right->addWidget(m_openCfgBtn);
    right->addStretch();

    auto *rightFrame = new QFrame(central);
    rightFrame->setObjectName(QStringLiteral("rightFrame"));
    rightFrame->setFrameShape(QFrame::StyledPanel);
    rightFrame->setMinimumWidth(250);
    rightFrame->setMaximumWidth(300);
    rightFrame->setLayout(right);

    // 左列
    auto *left = new QVBoxLayout();
    left->setSpacing(8);
    left->addWidget(m_chat, 1);
    {
        auto *row = new QHBoxLayout();
        row->setSpacing(6);
        row->addWidget(m_input, 1);
        row->addWidget(sendBtn);
        row->addWidget(m_clearBtn);
        left->addLayout(row);
    }
    left->addWidget(m_talkBtn);
    left->addWidget(m_status);

    auto *mainLay = new QHBoxLayout(central);
    mainLay->setContentsMargins(12, 12, 12, 12);
    mainLay->setSpacing(12);
    mainLay->addLayout(left, 1);
    mainLay->addWidget(rightFrame, 0);

    // 全局样式:粉色系女生风
    setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget { background:#fff5f7; color:#334155; font-size:14px;"
        "   font-family:\"Microsoft YaHei\",\"PingFang SC\",\"Segoe UI\",sans-serif; }"
        "QTextEdit { background:#ffffff; border:1px solid #fbcfe8; border-radius:12px; padding:8px; }"
        "QLineEdit { padding:8px 12px; border:1px solid #fbcfe8; border-radius:10px;"
        "   background:#ffffff; font-size:14px; }"
        "QLineEdit:focus { border:1px solid #ec4899; }"
        "QPushButton { padding:8px 16px; border:1px solid #f9a8d4; border-radius:10px; background:#ffffff;"
        "   color:#be185d; }"
        "QPushButton:hover { background:#fdf2f8; }"
        "QPushButton:pressed { background:#fbcfe8; }"
        "QPushButton:disabled { color:#f9a8d4; background:#fff1f2; }"
        "QPushButton#talkBtn { background:linear-gradient(90deg,#ec4899,#f472b6); color:#ffffff;"
        "   border:none; font-size:16px; font-weight:600; }"
        "QPushButton#talkBtn:hover { background:linear-gradient(90deg,#db2777,#ec4899); }"
        "QPushButton#talkBtn:pressed { background:linear-gradient(90deg,#be185d,#db2777); }"
        "QPushButton#talkBtn:disabled { background:#fbcfe8; color:#fecdd3; }"
        "QLabel { background:transparent; }"
        "QFrame#rightFrame { background:#ffffff; border:1px solid #fbcfe8; border-radius:12px; }"
    ));
    m_talkBtn->setObjectName(QStringLiteral("talkBtn"));

    // 信号连接
    connect(m_talkBtn, &QPushButton::pressed, this, &MainWindow::onTalkPressed);
    connect(m_talkBtn, &QPushButton::released, this, &MainWindow::onTalkReleased);
    connect(sendBtn,    &QPushButton::clicked, this, &MainWindow::onSendText);
    connect(m_input,    &QLineEdit::returnPressed, this, &MainWindow::onSendText);
    connect(m_clearBtn, &QPushButton::clicked, this, &MainWindow::onClearChat);
    connect(m_openCfgBtn, &QPushButton::clicked, this, &MainWindow::onOpenConfigFolder);
}

// ---------------- Pipeline 装配 ----------------

void MainWindow::setupPipeline()
{
    m_cfg->load();

    // 录音(始终可用)
    m_rec = new AudioRecorder(m_cfg->sampleRate(), this);
    connect(m_rec, &AudioRecorder::audioChunk, this, [this](const QByteArray &pcm) {
        if (m_vosk && m_vosk->isValid())
            m_vosk->feed(pcm);
    });
    connect(m_rec, &AudioRecorder::error, this, [this](const QString &m) { setStatus(m); });
    connect(m_rec, &AudioRecorder::recordingFinished, this, [this]() {
        if (!m_vosk || !m_vosk->isValid()) {
            setStatus(QStringLiteral("就绪(未配置 ASR,可用输入框)"));
            return;
        }
        setStatus(QStringLiteral("识别中…"));
        const QString text = m_vosk->finalResult().trimmed();
        m_vosk->reset();
        if (text.isEmpty()) {
            setStatus(QStringLiteral("没听清,再说一次"));
            return;
        }
        appendUserMessage(text);
        startAssistantTurn();
    });

    // Vosk(配置就绪才建)
    const DepCheck dc = m_cfg->checkAll();
    if (dc.vosk == DepStatus::Ok) {
        m_vosk = new VoskRecognizer(m_cfg->voskModelDir(), m_cfg->voskDllPath(),
                                    m_cfg->sampleRate(), this);
        connect(m_vosk, &VoskRecognizer::partialResult, this, [this](const QString &t) {
            if (!t.isEmpty())
                setStatus(QStringLiteral("听到:") + t);
        });
        connect(m_vosk, &VoskRecognizer::error, this, [this](const QString &e) {
            setStatus(QStringLiteral("ASR 错误:") + e);
            refreshDependencyPanel();
        });
        // 构造期错误(此时 error 信号未连),从 lastError 取回
        if (!m_vosk->isValid())
            setStatus(QStringLiteral("ASR 初始化失败:") + m_vosk->lastError());
    }

    // LLM(在线 API,始终建一个;连通性异步探)
    m_llm = new LlmClient(m_cfg->llmBaseUrl(), m_cfg->llmApiKey(), this);
    m_llm->setModel(m_cfg->llmModel());
    connect(m_llm, &LlmClient::chunkReceived, this, &MainWindow::appendAssistantChunk);
    connect(m_llm, &LlmClient::chatFinished, this, [this](const QString &full) {
        m_assistantBuf.clear();
        m_history.append({QStringLiteral("assistant"), full});
        renderChat();
        m_pipelineBusy = false;
        updateControlsEnabled();

        // 配置了 Piper 则朗读,否则只显示文本
        if (m_piper && QFileInfo::exists(m_cfg->piperExe())
            && QFileInfo::exists(m_cfg->piperVoice())) {
            setStatus(QStringLiteral("合成语音中…"));
            // 临时音频落 D 盘 _tmp,不写 C 盘 %TEMP%
            const QString tmp = m_cfg->tempDir() + QStringLiteral("/voiceai_reply.wav");
            m_piper->synthesize(full, tmp);
        } else {
            setStatus(QStringLiteral("就绪(未配置 TTS,仅显示文本)"));
        }
    });
    connect(m_llm, &LlmClient::chatError, this, [this](const QString &e) {
        m_assistantBuf.clear();
        renderChat();
        m_pipelineBusy = false;
        updateControlsEnabled();
        setStatus(QStringLiteral("LLM 错误:") + e);
    });
    connect(m_llm, &LlmClient::modelsListed, this,
            [this](const QStringList &names, bool ok, const QString &err) {
        if (ok) {
            m_llmInfo = QStringLiteral("在线(%1 个模型可用)").arg(names.size());
            if (!names.isEmpty() && !names.contains(m_llm->model()))
                m_llmInfo += QStringLiteral("<br>⚠ 未找到模型 ") + m_llm->model()
                           + QStringLiteral(",请核对 config.json 的 llm.model");
        } else {
            m_llmInfo = QStringLiteral("连接失败:%1").arg(err.toHtmlEscaped());
        }
        m_llmOnline = ok;
        refreshDependencyPanel();
        setStatus(QStringLiteral("就绪"));
    });

    // Piper(配置就绪才建)
    if (dc.piper == DepStatus::Ok) {
        PiperTts::Params pp;
        pp.lengthScale     = m_cfg->piperLengthScale();
        pp.noiseScale      = m_cfg->piperNoiseScale();
        pp.noiseW          = m_cfg->piperNoiseW();
        pp.sentenceSilence = m_cfg->piperSentenceSilence();
        m_piper = new PiperTts(m_cfg->piperExe(), m_cfg->piperVoice(), pp, this);
        connect(m_piper, &PiperTts::finished, this, [this](const QString &wav, bool ok) {
            if (ok) {
                setStatus(QStringLiteral("播放中…"));
                m_player->play(wav);
            } else {
                setStatus(QStringLiteral("就绪(TTS 失败)"));
            }
        });
        connect(m_piper, &PiperTts::error, this, [this](const QString &e) {
            setStatus(QStringLiteral("TTS 错误:") + e);
        });
    }

    // 播放(始终建)
    m_player = new AudioPlayer(this);
    connect(m_player, &AudioPlayer::playbackFinished, this, [this]() {
        setStatus(QStringLiteral("就绪"));
    });
    connect(m_player, &AudioPlayer::error, this, [this](const QString &e) {
        setStatus(QStringLiteral("播放错误:") + e);
    });

    refreshDependencyPanel();
    updateControlsEnabled();

    // 首屏开场白:小夏主动打招呼
    m_chat->setHtml(QStringLiteral(
        "<div style='display:flex; justify-content:flex-start; align-items:flex-start; margin:14px 4px 4px 4px;'>"
        "<div style='margin-right:8px; width:36px; height:36px; border-radius:50%%;"
        "   background:linear-gradient(135deg,#ec4899,#f9a8d4); color:#ffffff;"
        "   display:flex; align-items:center; justify-content:center; font-size:16px;'>💗</div>"
        "<div style='max-width:72%%;'>"
        "<div style='font-size:11px; color:#db2777; margin:0 0 3px 4px;'>小夏</div>"
        "<span style='display:inline-block; color:#831843; background:linear-gradient(90deg,#fff1f2,#fce7f3);"
        "padding:8px 14px; border-radius:14px 14px 14px 3px; white-space:pre-wrap; font-size:14px;"
        "border:1px solid #fbcfe8;'>"
        "嘿嘿,你来啦~ 人家等你好久了🥰<br>"
        "今天有没有想我呀? 想聊天就按住下面那个 <b>🎤 按住说话</b> 按钮哦,<br>"
        "不想说话也可以直接在输入框敲字~💖"
        "</span></div></div>"
        "<p style='color:#db2777; font-size:12px; padding:0 4px;'>"
        "✨ 小提示:她的脾气是娇滴滴的女朋友,别问她是不是AI啦~</p>"
    ));

    // 异步探 LLM 连通性(仅在已填 api_key 时探,避免无 key 也报错)
    if (!m_cfg->llmApiKey().isEmpty())
        m_llm->listModels();
}

// ---------------- 对话渲染 ----------------

void MainWindow::appendUserMessage(const QString &text)
{
    m_history.append({QStringLiteral("user"), text});
    m_assistantBuf.clear();
    renderChat();
}

void MainWindow::appendAssistantChunk(const QString &delta)
{
    m_assistantBuf += delta;
    renderChat();
}

void MainWindow::renderChat()
{
    // 头像:小夏(💗粉,左) 和 你(🧑蓝,右),emoji 头像不依赖图片文件
    static const QString userRow = QStringLiteral(
        "<div style='display:flex; justify-content:flex-end; align-items:flex-start; margin:10px 2px;'>"
        "<div style='max-width:72%%;'>"
        "<div style='font-size:11px; color:#3b82f6; margin:0 4px 3px 0; text-align:right;'>你</div>"
        "<span style='display:inline-block; color:#1e293b; background:#f1f5f9;"
        "padding:8px 14px; border-radius:14px 14px 3px 14px; white-space:pre-wrap; font-size:14px;"
        "font-weight:bold; border:1px solid #cbd5e1;'>%1</span>"
        "</div>"
        "<div style='margin-left:8px; width:36px; height:36px; border-radius:50%%;"
        "   background:linear-gradient(135deg,#3b82f6,#8b5cf6); color:#ffffff;"
        "   display:flex; align-items:center; justify-content:center; font-size:16px;'>🧑</div>"
        "</div>");
    static const QString asstRow = QStringLiteral(
        "<div style='display:flex; justify-content:flex-start; align-items:flex-start; margin:10px 2px;'>"
        "<div style='margin-right:8px; width:36px; height:36px; border-radius:50%%;"
        "   background:linear-gradient(135deg,#ec4899,#f9a8d4); color:#ffffff;"
        "   display:flex; align-items:center; justify-content:center; font-size:16px;'>💗</div>"
        "<div style='max-width:72%%;'>"
        "<div style='font-size:11px; color:#db2777; margin:0 0 3px 4px;'>小夏</div>"
        "<span style='display:inline-block; color:#831843; background:linear-gradient(90deg,#fff1f2,#fce7f3);"
        "padding:8px 14px; border-radius:14px 14px 14px 3px; white-space:pre-wrap; font-size:14px;"
        "border:1px solid #fbcfe8;'>%1</span>"
        "</div>"
        "</div>");

    QString html;
    for (const auto &m : m_history) {
        const QString body = m.second.toHtmlEscaped();
        html += (m.first == QLatin1String("user")) ? userRow.arg(body) : asstRow.arg(body);
    }
    if (!m_assistantBuf.isEmpty()) {
        html += asstRow.arg(m_assistantBuf.toHtmlEscaped()
                            + QStringLiteral(" <span style='color:#f472b6;'>▍</span>"));
    }
    m_chat->setHtml(html);
    // 滚到底
    if (auto *bar = m_chat->verticalScrollBar())
        bar->setValue(bar->maximum());
}

// ---------------- 交互 ----------------

void MainWindow::startAssistantTurn()
{
    if (!m_llm) {
        setStatus(QStringLiteral("LLM 未配置"));
        return;
    }
    m_assistantBuf.clear();
    m_pipelineBusy = true;
    updateControlsEnabled();
    setStatus(QStringLiteral("思考中…"));
    m_llm->chat(m_history);  // m_history 已含刚加入的用户消息
}

void MainWindow::onTalkPressed()
{
    if (m_pipelineBusy)
        return;
    if (!m_vosk || !m_vosk->isValid()) {
        setStatus(QStringLiteral("未配置 ASR,请用输入框"));
        return;
    }
    m_vosk->reset();
    setStatus(QStringLiteral("🎙 录音中…"));
    m_rec->start();
}

void MainWindow::onTalkReleased()
{
    if (!m_rec || !m_rec->isActive())
        return;
    setStatus(QStringLiteral("识别中…"));
    m_rec->stop();  // 同步触发 recordingFinished -> finalResult -> startAssistantTurn
}

void MainWindow::onSendText()
{
    if (m_pipelineBusy)
        return;
    const QString t = m_input->text().trimmed();
    if (t.isEmpty())
        return;
    m_input->clear();
    appendUserMessage(t);
    startAssistantTurn();
}

void MainWindow::onClearChat()
{
    if (m_pipelineBusy) {
        if (m_llm) m_llm->abort();
        m_pipelineBusy = false;
    }
    m_history.clear();
    m_assistantBuf.clear();
    renderChat();
    setStatus(QStringLiteral("已清空"));
    updateControlsEnabled();
}

void MainWindow::onOpenConfigFolder()
{
    // 优先打开资源根(放大文件的地方),其次才是配置目录
    const QString dir = m_cfg && !m_cfg->resourceRoot().isEmpty()
                             && QFileInfo::exists(m_cfg->resourceRoot())
                       ? m_cfg->resourceRoot() : m_cfg->configDir();
    if (!QFileInfo::exists(dir)) {
        QMessageBox::information(this, QStringLiteral("资源/配置文件夹"),
                                 QStringLiteral("路径不存在:%1").arg(dir));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

// ---------------- 状态/面板 ----------------

void MainWindow::setStatus(const QString &text)
{
    m_status->setText(text);
}

void MainWindow::refreshDependencyPanel()
{
    const DepCheck dc = m_cfg->checkAll();
    const auto color = [](DepStatus s) {
        if (s == DepStatus::Ok)        return QStringLiteral("#16a34a");
        if (s == DepStatus::Invalid)  return QStringLiteral("#d97706");
        return QStringLiteral("#dc2626");  // Missing
    };
    const auto mark = [](DepStatus s) {
        if (s == DepStatus::Ok)        return QStringLiteral("✓");
        if (s == DepStatus::Invalid)   return QStringLiteral("!");
        return QStringLiteral("✗");
    };

    // ASR 优先认 m_vosk 的真实加载结果(文件在但 dll 符号/模型损坏也能反映)
    const DepStatus vState = m_vosk ? (m_vosk->isValid() ? DepStatus::Ok : DepStatus::Invalid)
                                    : dc.vosk;
    const QString vDetail = m_vosk ? (m_vosk->isValid() ? QStringLiteral("就绪")
                                                        : m_vosk->lastError())
                                  : dc.messages.value(0);

    // LLM 状态:未填 key(红) / 检测中(橙) / 在线(绿) / 失败(红)
    QString llmLine;
    {
        const bool cfgOk = dc.llm == DepStatus::Ok;
        QString col, mk, info;
        if (!cfgOk) {
            col = QStringLiteral("#dc2626"); mk = QStringLiteral("✗");
            info = dc.messages.value(2);  // 未填 api_key
        } else if (m_llmInfo.isEmpty()) {
            col = QStringLiteral("#d97706"); mk = QStringLiteral("!");
            info = QStringLiteral("启动时检测中…");
        } else if (m_llmOnline) {
            col = QStringLiteral("#16a34a"); mk = QStringLiteral("✓");
            info = m_llmInfo;
        } else {
            col = QStringLiteral("#dc2626"); mk = QStringLiteral("✗");
            info = m_llmInfo;
        }
        llmLine = QStringLiteral("<b style='color:%1;'>%2 LLM(在线API)</b><br>"
                                 "<span style='color:#475569; font-size:12px;'>%3</span>")
                      .arg(col, mk, info);
    }

    const QString root = m_cfg ? m_cfg->resourceRoot().toHtmlEscaped() : QString();
    m_depLabel->setText(QStringLiteral(
        "<span style='color:#475569; font-size:12px;'>"
        "📁 资源根(资源优先放这里不占C盘):<br>"
        "<b>%8</b></span><br><br>"
        "<b style='color:%1;'>%2 ASR(Vosk)</b><br>"
        "<span style='color:#475569; font-size:12px;'>%3</span><br><br>"
        "<b style='color:%4;'>%5 TTS(Piper)</b><br>"
        "<span style='color:#475569; font-size:12px;'>%6</span><br><br>"
        "%7"
    ).arg(color(vState),  mark(vState),  vDetail,
          color(dc.piper), mark(dc.piper), dc.messages.value(1),
          llmLine, root));
}

void MainWindow::updateControlsEnabled()
{
    const bool canTalk = m_vosk && m_vosk->isValid() && !m_pipelineBusy;
    m_talkBtn->setEnabled(canTalk);
    m_talkBtn->setText(canTalk ? QStringLiteral("🎤 按住说话")
                       : (m_pipelineBusy ? QStringLiteral("处理中…")
                          : QStringLiteral("🎤 未配置 ASR")));
    m_input->setEnabled(!m_pipelineBusy);
}

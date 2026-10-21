#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// 各依赖的路径与参数(从 config.json 读取,空字段=自动探测)
// 单一资源根目录(留空=自动探测:D:\AI_Resources -> 项目根内默认目录)。
// 所有大文件(Vosk DLL/模型、Piper 引擎/语音、TTS 临时音频)优先走该目录,避免占 C 盘。
// 只有当资源根找不到时才回退到项目内默认目录(models/ runtime/ third_party/)。
struct VoskPaths {
    QString modelDir;   // 解压后的模型目录
    QString header;      // vosk_api.h(信息用)
    QString dll;         // libvosk.dll
};
struct PiperPaths {
    QString exe;         // piper.exe
    QString voice;       // *.onnx 语音模型
    double lengthScale      = 0.95;  // 默认稍快,更自然
    double noiseScale       = 0.8;   // 随机性(越大越不走调=像真人)
    double noiseW           = 1.0;   // 音素宽度噪声(让每个字不一样)
    double sentenceSilence  = 0.3;   // 句间停顿(默认0.3s不那么急)
};
// OpenAI 兼容的在线 LLM(DeepSeek / Groq / OpenAI 等)。
//   base_url 填到版本路径,例如:
//     DeepSeek: https://api.deepseek.com/v1
//     Groq:      https://api.groq.com/openai/v1
struct LlmApiPaths {
    QString baseUrl = QStringLiteral("https://api.deepseek.com/v1");
    QString apiKey;
    QString model   = QStringLiteral("deepseek-chat");
};
struct AudioConfig {
    int sampleRate = 16000;
};

enum class DepStatus { Ok, Missing, Invalid };

struct DepCheck {
    DepStatus vosk = DepStatus::Missing;
    DepStatus piper = DepStatus::Missing;
    DepStatus llm = DepStatus::Missing;   // 仅校验 api_key/base_url 是否填写,连通性由 MainWindow 异步探
    QStringList messages;                  // 每行说明
};

// 单一配置真相源:读写 config.json,提供解析后的路径,并做依赖存在性检查。
class ConfigManager : public QObject
{
    Q_OBJECT
public:
    explicit ConfigManager(QObject *parent = nullptr);

    void load();
    void save() const;

    // 解析后的路径(相对路径基于 config.json 所在目录;空字段自动探测)
    QString voskModelDir() const;
    QString voskDllPath() const;
    QString piperExe() const;
    QString piperVoice() const;
    double  piperLengthScale() const;
    double  piperNoiseScale() const;
    double  piperNoiseW() const;
    double  piperSentenceSilence() const;
    QString llmBaseUrl() const;
    QString llmApiKey() const;
    QString llmModel() const;
    int     sampleRate() const;

    // 临时目录(录音/TTS 中间文件落这里,避免写 C 盘 %TEMP%)。
    // 返回 resource_root/_tmp,首次调用自动建目录。
    QString tempDir() const;

    DepCheck checkAll() const;          // 启动时调用,驱动优雅降级

    QString configDir() const { return m_configDir; }       // config.json 所在目录(项目根)
    QString resourceRoot() const { return m_resourceRoot; } // 资源根(显式/D:\AI_Resources/项目根)
    QString configFilePath() const;

signals:
    void configChanged();

private:
    QString resolve(const QString &raw) const;               // 绝对/相对路径归一
    QString resolveUnderResource(const QString &relFromConfig,
                                 const QString &relFromRoot) const;
    QString autoDetectVoskModel() const;
    QString autoDetectVoskDll() const;
    QString autoDetectPiperExe() const;
    QString autoDetectPiperVoice() const;
    void locateConfigFile();                                  // 搜 app 目录/cwd/上级 找 config.json
    void resolveResourceRoot();                               // 解析显式或自动 D:\AI_Resources

    VoskPaths   m_vosk;
    PiperPaths  m_piper;
    LlmApiPaths m_llm;
    AudioConfig m_audio;
    QString m_rawResourceRoot;  // config.json 里写的原始值(空=自动)
    QString m_resourceRoot;     // 解析后:显式/D:\AI_Resources/configDir
    QString m_configDir;        // config.json 所在目录
};

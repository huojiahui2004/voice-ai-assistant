#include "ConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    locateConfigFile();
}

void ConfigManager::resolveResourceRoot()
{
    // 优先级:
    // 1. 显式 resource_root 字段
    // 2. configDir/resources/ 自包含(zip 发布包) — 只要 piper.exe 存在即视为完整
    // 3. D:\AI_Resources 存在或可建
    // 4. configDir(最后回退)
    if (!m_rawResourceRoot.isEmpty()) {
        m_resourceRoot = resolve(m_rawResourceRoot);
        return;
    }
    const QString embedded = QDir(m_configDir).absoluteFilePath(QStringLiteral("resources"));
    if (QDir(embedded).exists()
        && QFileInfo::exists(QDir(embedded).absoluteFilePath(QStringLiteral("piper/bin/piper.exe")))) {
        m_resourceRoot = QDir::toNativeSeparators(embedded);
        qInfo() << "[Config] 使用嵌入资源:" << m_resourceRoot;
        return;
    }
    static const QString kFallback = QStringLiteral("D:/AI_Resources");
    if (QDir(kFallback).exists() || QDir().mkpath(kFallback)) {
        m_resourceRoot = QDir::toNativeSeparators(QDir(kFallback).absolutePath());
    } else {
        m_resourceRoot = m_configDir;
    }
}

void ConfigManager::locateConfigFile()
{
    // 依次在 exe 目录、当前工作目录、exe 的上两级(开发期 build/ -> 项目根)查找 config.json
    const QStringList candidates = {
        QCoreApplication::applicationDirPath(),
        QDir::currentPath(),
        QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/..")).absolutePath(),
        QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/../..")).absolutePath(),
    };
    for (const QString &c : candidates) {
        if (QFileInfo::exists(QDir(c).absoluteFilePath(QStringLiteral("config.json")))) {
            m_configDir = QDir(c).absolutePath();
            return;
        }
    }
    // 找不到就用 exe 目录(部署时 config.json 与 exe 同目录)
    m_configDir = QCoreApplication::applicationDirPath();
}

QString ConfigManager::configFilePath() const
{
    return QDir(m_configDir).absoluteFilePath(QStringLiteral("config.json"));
}

void ConfigManager::load()
{
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[Config] 打不开" << configFilePath() << ",使用默认值";
        emit configChanged();
        return;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[Config] JSON 解析失败:" << err.errorString();
        emit configChanged();
        return;
    }
    const QJsonObject o = doc.object();

    const QJsonObject vosk = o.value(QStringLiteral("vosk")).toObject();
    m_vosk.modelDir = vosk.value(QStringLiteral("model_dir")).toString();
    m_vosk.header   = vosk.value(QStringLiteral("header")).toString();
    m_vosk.dll      = vosk.value(QStringLiteral("dll")).toString();

    const QJsonObject piper = o.value(QStringLiteral("piper")).toObject();
    m_piper.exe         = piper.value(QStringLiteral("exe")).toString();
    m_piper.voice       = piper.value(QStringLiteral("voice")).toString();
    m_piper.lengthScale = piper.value(QStringLiteral("length_scale")).toDouble(0.95);
    m_piper.noiseScale  = piper.value(QStringLiteral("noise_scale")).toDouble(0.8);
    m_piper.noiseW      = piper.value(QStringLiteral("noise_w")).toDouble(1.0);
    m_piper.sentenceSilence = piper.value(QStringLiteral("sentence_silence")).toDouble(0.3);

    const QJsonObject llm = o.value(QStringLiteral("llm")).toObject();
    m_llm.baseUrl = llm.value(QStringLiteral("base_url"))
                       .toString(QStringLiteral("https://api.deepseek.com/v1"));
    m_llm.apiKey  = llm.value(QStringLiteral("api_key")).toString();
    m_llm.model   = llm.value(QStringLiteral("model"))
                       .toString(QStringLiteral("deepseek-chat"));

    const QJsonObject audio = o.value(QStringLiteral("audio")).toObject();
    m_audio.sampleRate = audio.value(QStringLiteral("sample_rate")).toInt(16000);

    m_rawResourceRoot = o.value(QStringLiteral("resource_root")).toString();
    resolveResourceRoot();

    qInfo() << "[Config] 已加载:" << configFilePath()
            << "| resource_root=" << m_resourceRoot
            << "| vosk=" << m_vosk.modelDir
            << "| llm=" << m_llm.baseUrl << m_llm.model
            << "| api_key=" << (m_llm.apiKey.isEmpty() ? QStringLiteral("(空)")
                                                       : QStringLiteral("(已填)"));
    emit configChanged();
}

void ConfigManager::save() const
{
    QJsonObject o;
    QJsonObject vosk;
    vosk["model_dir"] = m_vosk.modelDir;
    vosk["header"]    = m_vosk.header;
    vosk["dll"]       = m_vosk.dll;
    o["vosk"] = vosk;

    QJsonObject piper;
    piper["exe"]         = m_piper.exe;
    piper["voice"]       = m_piper.voice;
    piper["length_scale"] = m_piper.lengthScale;
    piper["noise_scale"]  = m_piper.noiseScale;
    piper["noise_w"]      = m_piper.noiseW;
    piper["sentence_silence"] = m_piper.sentenceSilence;
    o["piper"] = piper;

    QJsonObject llm;
    llm["base_url"] = m_llm.baseUrl;
    llm["api_key"]  = m_llm.apiKey;
    llm["model"]    = m_llm.model;
    o["llm"] = llm;

    QJsonObject audio;
    audio["sample_rate"] = m_audio.sampleRate;
    o["audio"] = audio;

    o["resource_root"] = m_rawResourceRoot;

    QFile f(configFilePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

QString ConfigManager::resolve(const QString &raw) const
{
    if (raw.isEmpty())
        return {};
    const QFileInfo fi(raw);
    if (fi.isAbsolute())
        return QDir::toNativeSeparators(fi.absoluteFilePath());
    return QDir::toNativeSeparators(QDir(m_configDir).absoluteFilePath(raw));
}

// 先在 resource_root(通常 D:\AI_Resources)下找,找不到再回退到项目根内默认目录。
// 这样所有大文件都优先落到 D 盘,不占 C。
QString ConfigManager::resolveUnderResource(const QString &relFromRoot,
                                             const QString &relFromConfig) const
{
    const QString r = QDir(m_resourceRoot).absoluteFilePath(relFromRoot);
    if (QFileInfo::exists(r))
        return QDir::toNativeSeparators(r);
    const QString c = QDir(m_configDir).absoluteFilePath(relFromConfig);
    return QFileInfo::exists(c) ? QDir::toNativeSeparators(c) : QString();
}

QString ConfigManager::autoDetectVoskModel() const
{
    // 资源根下: resource_root/vosk/models/vosk-model-*
    // 项目内:    config/models/vosk/vosk-model-*
    const QString tryUnder = [&]() -> QString {
        const QDir base(QDir(m_resourceRoot).absoluteFilePath(QStringLiteral("vosk/models")));
        if (!base.exists()) return {};
        const QStringList ds = base.entryList(QStringList() << QStringLiteral("vosk-model-*"),
                                               QDir::Dirs | QDir::NoDotAndDotDot);
        return ds.isEmpty() ? QString()
                            : QDir::toNativeSeparators(base.absoluteFilePath(ds.first()));
    }();
    if (!tryUnder.isEmpty()) return tryUnder;
    const QDir base(QDir(m_configDir).absoluteFilePath(QStringLiteral("models/vosk")));
    if (!base.exists())
        return {};
    const QStringList dirs = base.entryList(QStringList() << QStringLiteral("vosk-model-*"),
                                             QDir::Dirs | QDir::NoDotAndDotDot);
    if (dirs.isEmpty())
        return {};
    return QDir::toNativeSeparators(base.absoluteFilePath(dirs.first()));
}

QString ConfigManager::autoDetectVoskDll() const
{
    // 1) exe 同目录(部署时拷贝),始终保留
    // 2) resource_root/vosk/sdk/libvosk.dll
    // 3) project third_party/vosk/libvosk.dll
    const QString exeDir = QDir(QCoreApplication::applicationDirPath())
                               .absoluteFilePath(QStringLiteral("libvosk.dll"));
    if (QFileInfo::exists(exeDir))
        return QDir::toNativeSeparators(exeDir);
    return resolveUnderResource(QStringLiteral("vosk/sdk/libvosk.dll"),
                                 QStringLiteral("third_party/vosk/libvosk.dll"));
}

QString ConfigManager::autoDetectPiperExe() const
{
    return resolveUnderResource(QStringLiteral("piper/bin/piper.exe"),
                                 QStringLiteral("runtime/piper/piper.exe"));
}

QString ConfigManager::autoDetectPiperVoice() const
{
    // 先 resource_root/piper/voices/*.onnx
    const QDir r(QDir(m_resourceRoot).absoluteFilePath(QStringLiteral("piper/voices")));
    if (r.exists()) {
        const QStringList fs = r.entryList(QStringList() << QStringLiteral("*.onnx"), QDir::Files);
        if (!fs.isEmpty())
            return QDir::toNativeSeparators(r.absoluteFilePath(fs.first()));
    }
    const QDir v(QDir(m_configDir).absoluteFilePath(QStringLiteral("runtime/piper/voices")));
    if (!v.exists())
        return {};
    const QStringList fs = v.entryList(QStringList() << QStringLiteral("*.onnx"), QDir::Files);
    if (fs.isEmpty())
        return {};
    return QDir::toNativeSeparators(v.absoluteFilePath(fs.first()));
}

QString ConfigManager::voskModelDir() const
{
    QString raw = m_vosk.modelDir.isEmpty() ? autoDetectVoskModel() : m_vosk.modelDir;
    return resolve(raw);
}

QString ConfigManager::voskDllPath() const
{
    QString raw = m_vosk.dll.isEmpty() ? autoDetectVoskDll() : m_vosk.dll;
    return resolve(raw);
}

QString ConfigManager::piperExe() const
{
    QString raw = m_piper.exe.isEmpty() ? autoDetectPiperExe() : m_piper.exe;
    return resolve(raw);
}

QString ConfigManager::piperVoice() const
{
    QString raw = m_piper.voice.isEmpty() ? autoDetectPiperVoice() : m_piper.voice;
    return resolve(raw);
}

double ConfigManager::piperLengthScale() const { return m_piper.lengthScale; }
double ConfigManager::piperNoiseScale() const { return m_piper.noiseScale; }
double ConfigManager::piperNoiseW() const { return m_piper.noiseW; }
double ConfigManager::piperSentenceSilence() const { return m_piper.sentenceSilence; }
QString ConfigManager::llmBaseUrl() const { return m_llm.baseUrl; }
QString ConfigManager::llmApiKey() const { return m_llm.apiKey; }
QString ConfigManager::llmModel() const { return m_llm.model; }
int     ConfigManager::sampleRate() const { return m_audio.sampleRate; }

QString ConfigManager::tempDir() const
{
    // 临时文件目录:优先 resource_root/_tmp(D 盘),回退 configDir/_tmp
    QString base = (!m_resourceRoot.isEmpty() && QDir(m_resourceRoot).exists())
                   ? m_resourceRoot : m_configDir;
    const QString tmp = QDir(base).absoluteFilePath(QStringLiteral("_tmp"));
    if (!QDir(tmp).exists())
        QDir().mkpath(tmp);
    return QDir::toNativeSeparators(tmp);
}

DepCheck ConfigManager::checkAll() const
{
    DepCheck c;

    // Vosk:模型目录存在且像模型 + dll 存在
    const QString model = voskModelDir();
    const QString dll = voskDllPath();
    const bool modelOk = !model.isEmpty() && QDir(model).exists()
                         && (QFileInfo::exists(model + QStringLiteral("/conf"))
                             || QFileInfo::exists(model + QStringLiteral("/graph"))
                             || QFileInfo::exists(model + QStringLiteral("/ivector")));
    const bool dllOk = !dll.isEmpty() && QFileInfo::exists(dll);
    c.vosk = (modelOk && dllOk) ? DepStatus::Ok
             : (modelOk ? DepStatus::Invalid : DepStatus::Missing);
    c.messages << (c.vosk == DepStatus::Ok ? QStringLiteral("就绪")
                   : (model.isEmpty() ? QStringLiteral("未放模型")
                      : QStringLiteral("模型目录无效或缺少 libvosk.dll")));

    // Piper:exe + onnx + onnx.json 同在
    const QString exe = piperExe();
    const QString voice = piperVoice();
    const bool exeOk = !exe.isEmpty() && QFileInfo::exists(exe);
    const bool voiceOk = !voice.isEmpty() && QFileInfo::exists(voice)
                         && QFileInfo::exists(voice + QStringLiteral(".json"));
    c.piper = (exeOk && voiceOk) ? DepStatus::Ok
              : (exeOk || voiceOk ? DepStatus::Invalid : DepStatus::Missing);
    c.messages << (c.piper == DepStatus::Ok ? QStringLiteral("就绪")
                   : (exe.isEmpty() ? QStringLiteral("未放 piper.exe/语音")
                      : QStringLiteral("缺语音 .onnx/.onnx.json")));

    // LLM:仅校验 api_key + base_url 填写,连通性由 MainWindow 异步探
    const bool cfgOk = !m_llm.apiKey.isEmpty() && !m_llm.baseUrl.isEmpty();
    c.llm = cfgOk ? DepStatus::Ok : DepStatus::Missing;
    c.messages << (cfgOk ? QStringLiteral("已配置(待连通性探活)")
                   : QStringLiteral("未填 api_key"));

    return c;
}

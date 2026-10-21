#include "VoskRecognizer.h"

#include <QLibrary>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QDebug>

// ---- Vosk C API 函数指针(签名依据官方 vosk_api.h,extern "C") ----
namespace {
using VoskModel = void;
using VoskRec = void;

typedef VoskModel* (*FnModelNew)(const char *model_path);
typedef void (*FnModelFree)(VoskModel *model);
typedef VoskRec* (*FnRecNew)(VoskModel *model, float sample_rate);
typedef void (*FnRecFree)(VoskRec *recognizer);
typedef int  (*FnAccept)(VoskRec *recognizer, const char *data, int length);
typedef const char *(*FnResult)(VoskRec *recognizer);
typedef const char *(*FnPartial)(VoskRec *recognizer);
typedef const char *(*FnFinal)(VoskRec *recognizer);
typedef void (*FnReset)(VoskRec *recognizer);
typedef void (*FnSetLogLevel)(int log_level);

struct VoskApi {
    FnModelNew   model_new = nullptr;
    FnModelFree  model_free = nullptr;
    FnRecNew     rec_new = nullptr;
    FnRecFree    rec_free = nullptr;
    FnAccept     accept = nullptr;
    FnResult     result = nullptr;
    FnPartial    partial = nullptr;
    FnFinal      final = nullptr;
    FnReset      reset = nullptr;
    FnSetLogLevel set_log_level = nullptr;
};

// accept_waveform 返回值含义:1=检测到静音/可取 result;0=继续;-1=异常
} // namespace

static QString extractJsonField(const char *json, const char *key)
{
    if (!json)
        return {};
    const QJsonDocument d = QJsonDocument::fromJson(QByteArray(json));  // QByteArray 做 strlen
    return d.isObject() ? d.object().value(QString::fromLatin1(key)).toString() : QString();
}

void VoskRecognizer::fail(const QString &msg)
{
    m_lastError = msg;
    emit error(msg);
}

VoskRecognizer::VoskRecognizer(const QString &modelDir, const QString &dllPath,
                               int sampleRate, QObject *parent)
    : QObject(parent)
{
    m_lib = new QLibrary(this);

    // 解析 dll:优先显式路径,否则按名 libvosk 搜索(exe 目录/PATH)
    if (!dllPath.isEmpty()) {
        m_lib->setFileName(dllPath);
    } else {
        m_lib->setFileName(QStringLiteral("libvosk"));
    }
    if (!m_lib->load()) {
        QString firstErr = m_lib->errorString();
        // 兜底:按名补 .dll 再试
        m_lib->setFileName(QStringLiteral("libvosk.dll"));
        if (!m_lib->load()) {
            fail(QStringLiteral("无法加载 libvosk.dll:%1。"
                                "请从 https://github.com/alphacep/vosk-api/releases "
                                "下载并放到 exe 同目录。").arg(firstErr));
            return;
        }
    }

    auto *api = new VoskApi;
    api->model_new   = reinterpret_cast<FnModelNew>(m_lib->resolve("vosk_model_new"));
    api->model_free  = reinterpret_cast<FnModelFree>(m_lib->resolve("vosk_model_free"));
    api->rec_new     = reinterpret_cast<FnRecNew>(m_lib->resolve("vosk_recognizer_new"));
    api->rec_free    = reinterpret_cast<FnRecFree>(m_lib->resolve("vosk_recognizer_free"));
    api->accept      = reinterpret_cast<FnAccept>(m_lib->resolve("vosk_recognizer_accept_waveform"));
    api->result      = reinterpret_cast<FnResult>(m_lib->resolve("vosk_recognizer_result"));
    api->partial     = reinterpret_cast<FnPartial>(m_lib->resolve("vosk_recognizer_partial_result"));
    api->final       = reinterpret_cast<FnFinal>(m_lib->resolve("vosk_recognizer_final_result"));
    api->reset       = reinterpret_cast<FnReset>(m_lib->resolve("vosk_recognizer_reset"));
    api->set_log_level = reinterpret_cast<FnSetLogLevel>(m_lib->resolve("vosk_set_log_level"));
    m_api = api;

    if (!api->model_new || !api->rec_new || !api->accept || !api->final) {
        fail(QStringLiteral("libvosk.dll 缺少必需符号(版本过旧?)"));
        return;
    }
    if (api->set_log_level)
        api->set_log_level(-1);  // 抑制 kaldi 噪音日志

    if (modelDir.isEmpty() || !QDir(modelDir).exists()) {
        fail(QStringLiteral("Vosk 模型目录不存在:%1").arg(modelDir));
        return;
    }
    const QByteArray modelPath = QDir::toNativeSeparators(modelDir).toUtf8();
    m_model = api->model_new(modelPath.constData());
    if (!m_model) {
        fail(QStringLiteral("vosk_model_new 失败,模型路径:%1").arg(modelDir));
        return;
    }
    m_rec = api->rec_new(reinterpret_cast<VoskModel*>(m_model), float(sampleRate));
    if (!m_rec) {
        api->model_free(reinterpret_cast<VoskModel*>(m_model));
        m_model = nullptr;
        fail(QStringLiteral("vosk_recognizer_new 失败"));
        return;
    }
    m_valid = true;
    qInfo() << "[Vosk] 就绪,模型:" << modelDir << "采样率" << sampleRate;
}

VoskRecognizer::~VoskRecognizer()
{
    if (m_api) {
        auto *api = static_cast<VoskApi*>(m_api);
        if (m_rec)     api->rec_free(reinterpret_cast<VoskRec*>(m_rec));
        if (m_model)   api->model_free(reinterpret_cast<VoskModel*>(m_model));
        delete api;
        m_api = nullptr;
    }
    // m_lib 是 this 的子对象,自动释放
}

void VoskRecognizer::feed(const QByteArray &pcm)
{
    if (!m_valid || !m_api || pcm.isEmpty())
        return;
    auto *api = static_cast<VoskApi*>(m_api);
    const int ret = api->accept(reinterpret_cast<VoskRec*>(m_rec),
                                pcm.constData(), pcm.size());
    // 实时部分结果
    if (api->partial) {
        const char *p = api->partial(reinterpret_cast<VoskRec*>(m_rec));
        const QString t = extractJsonField(p, "partial");
        if (!t.isEmpty())
            emit partialResult(t);
    }
    if (ret == 1) {
        // 检测到静音边界:取该段最终文本并累积
        const char *r = api->result(reinterpret_cast<VoskRec*>(m_rec));
        const QString t = extractJsonField(r, "text").trimmed();
        if (!t.isEmpty())
            m_accumulated += (m_accumulated.isEmpty() ? QString() : QStringLiteral(" ")) + t;
    }
}

QString VoskRecognizer::finalResult()
{
    if (!m_valid || !m_api)
        return {};
    auto *api = static_cast<VoskApi*>(m_api);
    const char *f = api->final(reinterpret_cast<VoskRec*>(m_rec));
    const QString t = extractJsonField(f, "text").trimmed();
    if (!t.isEmpty())
        m_accumulated += (m_accumulated.isEmpty() ? QString() : QStringLiteral(" ")) + t;
    const QString result = m_accumulated;
    m_accumulated.clear();
    if (api->reset)
        api->reset(reinterpret_cast<VoskRec*>(m_rec));
    return result;
}

void VoskRecognizer::reset()
{
    if (m_api) {
        auto *api = static_cast<VoskApi*>(m_api);
        if (api->reset && m_rec)
            api->reset(reinterpret_cast<VoskRec*>(m_rec));
    }
    m_accumulated.clear();
}

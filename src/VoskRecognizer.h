#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

class QLibrary;

// Vosk 离线识别封装。
// libvosk.dll 在运行期由 QLibrary 动态加载:未放 dll 也能编译与启动(此时 isValid()==false)。
class VoskRecognizer : public QObject
{
    Q_OBJECT
public:
    // modelDir:解压后的模型目录(非 zip); dllPath:libvosk.dll 路径(空=系统/同名搜索)
    VoskRecognizer(const QString &modelDir, const QString &dllPath,
                   int sampleRate = 16000, QObject *parent = nullptr);
    ~VoskRecognizer() override;

    bool isValid() const { return m_valid; }
    // 构造期错误(此时 error 信号尚未连接,故同时存为成员供外部读取)
    QString lastError() const { return m_lastError; }

    void feed(const QByteArray &pcm);   // 喂 PCM 16-bit mono 字节,内部累积识别
    QString finalResult();              // 取最终完整文本并 reset
    void reset();

signals:
    void partialResult(const QString &text);   // 实时部分结果
    void finalResultReady(const QString &text);// finalResult 取出时
    void error(const QString &msg);

private:
    void fail(const QString &msg);       // 存 lastError 并 emit error

    QLibrary *m_lib = nullptr;
    void *m_api = nullptr;   // VoskApi* (定义在 cpp)
    void *m_model = nullptr;
    void *m_rec = nullptr;
    bool m_valid = false;
    QString m_lastError;
    QString m_accumulated;   // 跨静音边界的累积文本
};

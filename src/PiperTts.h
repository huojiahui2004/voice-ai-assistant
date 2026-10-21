#pragma once

#include <QObject>
#include <QString>

class QProcess;

// 调用 piper.exe 子进程合成语音:文本经 stdin(UTF-8)输入,输出 WAV 文件。
// onnxruntime/espeak-ng 随 piper 发布包提供,无需额外链接。
class PiperTts : public QObject
{
    Q_OBJECT
public:
    struct Params {
        double lengthScale = 0.95;
        double noiseScale  = 0.8;
        double noiseW      = 1.0;
        double sentenceSilence = 0.3;
    };

    PiperTts(const QString &piperExe, const QString &voiceOnnx,
             const Params &params, QObject *parent = nullptr);

    void synthesize(const QString &text, const QString &outWavPath);
    bool isBusy() const { return m_busy; }

signals:
    void finished(const QString &wavPath, bool ok);
    void error(const QString &msg);

private:
    const QString m_exe;
    const QString m_voice;
    const Params  m_params;
    QProcess *m_proc = nullptr;
    bool m_busy = false;
    QString m_outWav;
};

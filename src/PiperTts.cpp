#include "PiperTts.h"

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

PiperTts::PiperTts(const QString &piperExe, const QString &voiceOnnx,
                   const Params &params, QObject *parent)
    : QObject(parent)
    , m_exe(piperExe)
    , m_voice(voiceOnnx)
    , m_params(params)
{
}

void PiperTts::synthesize(const QString &text, const QString &outWavPath)
{
    if (m_busy) {
        emit error(QStringLiteral("Piper 正忙,忽略新请求"));
        return;
    }
    if (m_exe.isEmpty() || !QFileInfo::exists(m_exe)) {
        emit error(QStringLiteral("piper.exe 不存在:%1").arg(m_exe));
        return;
    }
    if (m_voice.isEmpty() || !QFileInfo::exists(m_voice)) {
        emit error(QStringLiteral("Piper 语音模型不存在:%1").arg(m_voice));
        return;
    }
    if (text.trimmed().isEmpty()) {
        emit error(QStringLiteral("合成文本为空"));
        return;
    }

    m_outWav = outWavPath;
    m_busy = true;

    if (!m_proc) {
        m_proc = new QProcess(this);
        connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int code, QProcess::ExitStatus) {
            const bool ok = (code == 0) && QFileInfo::exists(m_outWav);
            m_busy = false;
            if (ok)
                emit finished(m_outWav, true);
            else
                emit error(QStringLiteral("Piper 退出码 %1").arg(code));
        });
        connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
            m_busy = false;
            emit error(QStringLiteral("Piper 启动失败:%1").arg(m_proc->errorString()));
        });
    }

    // 工作目录设为 piper.exe 所在目录,保证 espeak-ng-data 相对路径生效
    m_proc->setWorkingDirectory(QFileInfo(m_exe).absolutePath());

    QStringList args;
    args << QStringLiteral("--model") << m_voice
         << QStringLiteral("--output_file") << outWavPath
         << QStringLiteral("--length_scale")   << QString::number(m_params.lengthScale, 'f', 3)
         << QStringLiteral("--noise_scale")    << QString::number(m_params.noiseScale, 'f', 3)
         << QStringLiteral("--noise_w")        << QString::number(m_params.noiseW, 'f', 3)
         << QStringLiteral("--sentence_silence") << QString::number(m_params.sentenceSilence, 'f', 3);
    m_proc->start(m_exe, args);
    if (!m_proc->waitForStarted(10000)) {
        m_busy = false;
        emit error(QStringLiteral("Piper 启动超时:%1").arg(m_proc->errorString()));
        return;
    }
    // 文本经 stdin(UTF-8)传入,关闭写通道表示 EOF
    m_proc->write(text.toUtf8());
    m_proc->closeWriteChannel();
    qInfo() << "[Piper] 合成:" << text.left(40) << "... -> " << outWavPath;
}

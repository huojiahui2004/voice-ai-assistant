#pragma once

#include <QObject>
#include <QByteArray>

class QAudioSource;
class QIODevice;

// Qt6 QAudioSource 封装:采集 16k/mono/Int16 PCM,按 readyRead 发 audioChunk。
// (Qt6 中 QAudioInput 已废弃,低层 PCM 采集用 QAudioSource。)
class AudioRecorder : public QObject
{
    Q_OBJECT
public:
    explicit AudioRecorder(int sampleRate = 16000, QObject *parent = nullptr);
    ~AudioRecorder() override;

    void start();
    void stop();
    bool isActive() const { return m_source != nullptr; }

signals:
    void audioChunk(const QByteArray &pcm);  // PCM 16-bit mono 字节
    void recordingFinished();
    void error(const QString &msg);

private:
    const int m_sampleRate;
    QAudioSource *m_source = nullptr;
    QIODevice *m_io = nullptr;
};

#include "AudioRecorder.h"

#include <QAudioSource>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QIODevice>
#include <QDebug>

AudioRecorder::AudioRecorder(int sampleRate, QObject *parent)
    : QObject(parent)
    , m_sampleRate(sampleRate)
{
}

AudioRecorder::~AudioRecorder()
{
    stop();
}

void AudioRecorder::start()
{
    if (m_source)
        return;

    QAudioFormat format;
    format.setSampleRate(m_sampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);  // Vosk 要求 16-bit mono

    const QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (dev.isNull()) {
        emit error(QStringLiteral("找不到音频输入设备(麦克风)"));
        return;
    }
    // Qt6 一般能自动重采样;不强校验 isFormatSupported。

    m_source = new QAudioSource(dev, format, this);
    m_io = m_source->start();
    if (!m_io) {
        emit error(QStringLiteral("音频源启动失败"));
        m_source->deleteLater();
        m_source = nullptr;
        return;
    }
    connect(m_io, &QIODevice::readyRead, this, [this]() {
        const QByteArray data = m_io->readAll();
        if (!data.isEmpty())
            emit audioChunk(data);
    });
    qInfo() << "[Audio] 开始录音" << m_sampleRate << "Hz";
}

void AudioRecorder::stop()
{
    if (!m_source)
        return;
    // 冲刷缓冲中剩余字节,确保识别器拿到完整尾音
    if (m_io) {
        const QByteArray data = m_io->readAll();
        if (!data.isEmpty())
            emit audioChunk(data);
        m_io = nullptr;
    }
    m_source->stop();
    m_source->deleteLater();
    m_source = nullptr;
    emit recordingFinished();
    qInfo() << "[Audio] 停止录音";
}

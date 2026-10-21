#include "AudioPlayer.h"

#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>
#include <QFileInfo>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_out(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_out);  // Qt6 必需,否则无声

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus s) {
        if (s == QMediaPlayer::EndOfMedia)
            emit playbackFinished();
    });
    connect(m_player, &QMediaPlayer::errorChanged, this, [this]() {
        if (m_player->error() != QMediaPlayer::NoError)
            emit error(m_player->errorString());
    });
}

void AudioPlayer::play(const QString &wavPath)
{
    if (!QFileInfo::exists(wavPath)) {
        emit error(QStringLiteral("音频文件不存在:%1").arg(wavPath));
        return;
    }
    m_player->setSource(QUrl::fromLocalFile(wavPath));
    m_player->play();
}

void AudioPlayer::stop()
{
    m_player->stop();
}

#pragma once

#include <QObject>
#include <QString>

class QMediaPlayer;
class QAudioOutput;

// Qt6 播放 WAV:QMediaPlayer 必须配 QAudioOutput 并 setAudioOutput,否则无声。
class AudioPlayer : public QObject
{
    Q_OBJECT
public:
    explicit AudioPlayer(QObject *parent = nullptr);

    void play(const QString &wavPath);
    void stop();

signals:
    void playbackFinished();
    void error(const QString &msg);

private:
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_out = nullptr;
};

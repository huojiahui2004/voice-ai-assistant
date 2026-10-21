#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>

class QNetworkAccessManager;
class QNetworkReply;

// OpenAI 兼容的聊天客户端(支持 DeepSeek / Groq / OpenAI 等任何兼容服务)。
//   POST {base_url}/chat/completions  (stream=true)
//   GET  {base_url}/models            (探活 + 列模型)
// base_url 填到版本路径,例如:
//   DeepSeek: https://api.deepseek.com/v1        model: deepseek-chat
//   Groq:      https://api.groq.com/openai/v1     model: llama-3.3-70b-versatile
class LlmClient : public QObject
{
    Q_OBJECT
public:
    using Message = QPair<QString, QString>;  // role, content

    explicit LlmClient(const QString &baseUrl, const QString &apiKey, QObject *parent = nullptr);

    void setModel(const QString &model);
    QString model() const { return m_model; }

    void chat(const QList<Message> &messages);  // stream=true
    void listModels();                          // GET /models (探活)
    void abort();

    // 人设:system prompt 注入,不影响外部 m_history
    static QString defaultPersonaPrompt();

signals:
    void chunkReceived(const QString &delta);
    void chatFinished(const QString &fullText);
    void chatError(const QString &msg);
    void modelsListed(const QStringList &names, bool ok, const QString &err);

private:
    QNetworkAccessManager *m_nam;
    QString m_baseUrl;
    QString m_apiKey;
    QString m_model = QStringLiteral("deepseek-chat");
    QNetworkReply *m_chatReply = nullptr;
    QByteArray m_buf;
    QString m_full;
    bool m_doneEmitted = false;
};

@echo off
chcp 65001 >nul
title AI 资源部署到 D 盘(在线 LLM 方案)

echo ========================================
echo   AI 资源部署到 D 盘
echo   (LLM 走在线 API,不下载 4.7GB 模型)
echo   ASR/TTS 仍为本地,资源放 D 盘不占 C 盘
echo ========================================
echo.

REM ---- 步骤 1:创建 D:\AI_Resources 目录结构(Vosk / Piper 放这里)----
echo [1/2] 创建 D:\AI_Resources 目录结构...
if not exist "D:\AI_Resources\vosk\sdk"      mkdir "D:\AI_Resources\vosk\sdk"
if not exist "D:\AI_Resources\vosk\models"    mkdir "D:\AI_Resources\vosk\models"
if not exist "D:\AI_Resources\piper\bin"      mkdir "D:\AI_Resources\piper\bin"
if not exist "D:\AI_Resources\piper\voices"  mkdir "D:\AI_Resources\piper\voices"
if not exist "D:\AI_Resources\_downloads"    mkdir "D:\AI_Resources\_downloads"
if not exist "D:\AI_Resources\_tmp"           mkdir "D:\AI_Resources\_tmp"
echo   完成: D:\AI_Resources\(vosk, piper, _downloads, _tmp 已建好)
echo   _tmp 是程序运行时临时音频目录(替代 C 盘 %%TEMP%%)
echo.

REM ---- 步骤 2:打开 config.json 提醒填 API key ----
echo [2/2] 配置 LLM API key...
echo.
echo   LLM 已改为在线 API 方案(不再用本地 Ollama,不下载 4.7GB)。
echo   二选一(都兼容 OpenAI 接口):
echo.
echo   [DeepSeek 推荐,国内直连无需代理]
echo     1. 打开 https://platform.deepseek.com 注册登录
echo     2. 充值(最少 1 元即可,deepseek-chat 很便宜,约 0.001 元/千 token)
echo     3. 左侧 "API Keys" -> 创建 API Key,复制
echo     4. 编辑 d:\friend\config.json,填入:
echo        "llm": {
echo          "base_url": "https://api.deepseek.com/v1",
echo          "api_key": "sk-你的key",
echo          "model": "deepseek-chat"
echo        }
echo.
echo   [Groq 免费,需代理访问]
echo     1. 打开 https://console.groq.com 注册(可用 Google 登录)
echo     2. 左侧 "API Keys" -> Create API Key,复制
echo     3. 编辑 d:\friend\config.json,填入:
echo        "llm": {
echo          "base_url": "https://api.groq.com/openai/v1",
echo          "api_key": "gsk_你的key",
echo          "model": "llama-3.3-70b-versatile"
echo        }
echo.
echo   填好后保存 config.json,运行 d:\friend\build\friend.exe 即可。
echo.

echo ========================================
echo   接下来手动下载 Vosk / Piper(需浏览器,放 D 盘)
echo ========================================
echo.
echo [Vosk ASR]
echo   1. 浏览器打开 https://github.com/alphacep/vosk-api/releases
echo      下载 vosk-win64-0.3.45.zip,保存到 D:\AI_Resources\_downloads\
echo      解压,把 vosk_api.h 和 libvosk.dll 放到 D:\AI_Resources\vosk\sdk\
echo   2. 浏览器打开 https://alphacephei.com/vosk/models
echo      下载 vosk-model-small-cn-0.22.zip,保存到 D:\AI_Resources\_downloads\
echo      解压出的整个目录放到 D:\AI_Resources\vosk\models\
echo.
echo [Piper TTS]
echo   1. 浏览器打开 https://github.com/rhasspy/piper/releases
echo      下载 piper_windows_amd64.zip,保存到 D:\AI_Resources\_downloads\
echo      解压 piper.exe / onnxruntime.dll / espeak-ng.dll / espeak-ng-data 到 D:\AI_Resources\piper\bin\
echo   2. 浏览器打开 https://huggingface.co/rhasspy/piper-voices/tree/v1.0.0/zh/zh_CN/huayan/medium
echo      下载 zh_CN-huayan-medium.onnx 和 zh_CN-huayan-medium.onnx.json 两个文件
echo      放到 D:\AI_Resources\piper\voices\
echo.
echo ========================================
echo   全部完成!
echo   - config.json 填好 api_key
echo   - Vosk/Piper 文件放好
echo   - 运行 d:\friend\build\friend.exe
echo ========================================
echo.
pause

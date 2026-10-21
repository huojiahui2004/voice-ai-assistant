@echo off
chcp 65001 >nul
title 一键发布: 构建 friend-release.zip

echo ========================================
echo   打包 friend-release.zip
echo   (Release 版 + 全资源自包含)
echo ========================================
echo.

REM ---- 清理旧发布目录 ----
set PKG_DIR=d:\friend\release-package
set BUILD_DIR=d:\friend\build_release
set RES_DIR=D:\AI_Resources

echo [1/6] 清理旧发布目录...
if exist "%PKG_DIR%" rmdir /S /Q "%PKG_DIR%"
if exist "d:\friend\friend-release.zip" del /Q "d:\friend\friend-release.zip"
mkdir "%PKG_DIR%"
echo   完成
echo.

REM ---- 2. 复制程序运行时(friend.exe + Qt DLL + MinGW runtime + plugins + libvosk.dll)----
echo [2/6] 复制程序运行时...
cd /d "%BUILD_DIR%"
for %%f in (friend.exe Qt6Core.dll Qt6Gui.dll Qt6Multimedia.dll Qt6Network.dll Qt6Svg.dll Qt6Widgets.dll avcodec-61.dll avformat-61.dll avutil-59.dll swresample-5.dll swscale-8.dll D3Dcompiler_47.dll opengl32sw.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libvosk.dll) do (
    if exist "%%f" copy /Y "%%f" "%PKG_DIR%\" >nul
)
echo   - friend.exe + Qt runtime + ffmpeg + libvosk.dll
echo.

REM ---- 3. 复制 Qt 插件子目录 ----
echo [3/6] 复制 Qt 插件...
for %%d in (generic iconengines imageformats multimedia networkinformation platforms styles tls translations) do (
    if exist "%%d" (
        xcopy "%%d" "%PKG_DIR%\%%d\" /E /I /Y >nul
    )
)
echo   - platforms/multimedia/imageformats/.../translations
echo.

REM ---- 4. 复制干净 config.json ----
echo [4/6] 写入干净 config.json(api_key 空)...
copy /Y "d:\friend\build_release\config.json" "%PKG_DIR%\config.json" >nul
echo   完成
echo.

REM ---- 5. 复制资源(Vosk 模型 + Piper 引擎 + Piper 语音)----
echo [5/6] 复制本地资源(打包为自包含)...
echo   - Vosk SDK
if not exist "%PKG_DIR%\resources\vosk\sdk" mkdir "%PKG_DIR%\resources\vosk\sdk"
copy /Y "%RES_DIR%\vosk\sdk\libvosk.dll" "%PKG_DIR%\resources\vosk\sdk\" >nul
copy /Y "%RES_DIR%\vosk\sdk\vosk_api.h" "%PKG_DIR%\resources\vosk\sdk\" >nul

echo   - Vosk 中文模型
if not exist "%PKG_DIR%\resources\vosk\models" mkdir "%PKG_DIR%\resources\vosk\models"
xcopy "%RES_DIR%\vosk\models\vosk-model-small-cn-0.22" "%PKG_DIR%\resources\vosk\models\vosk-model-small-cn-0.22\" /E /I /Y >nul

echo   - Piper 引擎
if not exist "%PKG_DIR%\resources\piper\bin" mkdir "%PKG_DIR%\resources\piper\bin"
xcopy "%RES_DIR%\piper\bin\*" "%PKG_DIR%\resources\piper\bin\" /E /I /Y >nul

echo   - Piper 中文语音
if not exist "%PKG_DIR%\resources\piper\voices" mkdir "%PKG_DIR%\resources\piper\voices"
copy /Y "%RES_DIR%\piper\voices\*.onnx" "%PKG_DIR%\resources\piper\voices\" >nul
copy /Y "%RES_DIR%\piper\voices\*.onnx.json" "%PKG_DIR%\resources\piper\voices\" >nul

echo   - 临时文件目录
mkdir "%PKG_DIR%\_tmp"

echo   完成
echo.

REM ---- 6. 写一个启动说明 + 生成 zip ----
echo [6/6] 生成启动说明 + 压缩 zip...

echo friend - 语音 AI 助手 > "%PKG_DIR%\使用说明.txt"
echo ======================== >> "%PKG_DIR%\使用说明.txt"
echo. >> "%PKG_DIR%\使用说明.txt"
echo 首次使用: >> "%PKG_DIR%\使用说明.txt"
echo   1. 用记事本打开 config.json >> "%PKG_DIR%\使用说明.txt"
echo   2. 在 llm.api_key 填入你的 DeepSeek API key >> "%PKG_DIR%\使用说明.txt"
echo      (申请地址: https://platform.deepseek.com) >> "%PKG_DIR%\使用说明.txt"
echo   3. 保存,双击 friend.exe 启动 >> "%PKG_DIR%\使用说明.txt"
echo. >> "%PKG_DIR%\使用说明.txt"
echo 资源目录: resources\ (已包含 Vosk 模型 + Piper 引擎 + 中文语音) >> "%PKG_DIR%\使用说明.txt"
echo 临时文件: _tmp\ (录音 / TTS 中间文件,自动清理) >> "%PKG_DIR%\使用说明.txt"
echo. >> "%PKG_DIR%\使用说明.txt"
echo 使用: >> "%PKG_DIR%\使用说明.txt"
echo   - 按住 ^<F2^> 或点击按钮说话,松开识别并发问 >> "%PKG_DIR%\使用说明.txt"
echo   - 聊天框内 Ctrl+C 复制,Ctrl+V 粘贴,Esc 取消当前对话 >> "%PKG_DIR%\使用说明.txt"

cd /d d:\friend
powershell -NoProfile -Command "Compress-Archive -Path 'release-package\*' -DestinationPath 'friend-release.zip' -Force"

echo.
echo ========================================
echo   发布完成!
echo   发布目录: %PKG_DIR%
echo   zip 文件: d:\friend\friend-release.zip
echo ========================================
echo.

REM ---- 显示 zip 大小 ----
for %%f in (d:\friend\friend-release.zip) do (
    echo zip 大小: %%~zf bytes / [math]::Round(%%~zf/1MB,1) MB
)
echo.

pause

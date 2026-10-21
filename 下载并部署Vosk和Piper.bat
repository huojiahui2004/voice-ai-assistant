@echo off
chcp 65001 >nul
title 自动下载并部署 Vosk + Piper 到 D:\AI_Resources

echo ========================================
echo   自动下载并部署 Vosk + Piper
echo   (无需手动操作,zip 下载后会自动解压)
echo ========================================
echo.

REM ---- 创建目录 ----
echo [1/8] 创建 D:\AI_Resources 目录结构...
if not exist "D:\AI_Resources\vosk\sdk"     mkdir "D:\AI_Resources\vosk\sdk"
if not exist "D:\AI_Resources\vosk\models"  mkdir "D:\AI_Resources\vosk\models"
if not exist "D:\AI_Resources\piper\bin"   mkdir "D:\AI_Resources\piper\bin"
if not exist "D:\AI_Resources\piper\voices" mkdir "D:\AI_Resources\piper\voices"
if not exist "D:\AI_Resources\_downloads"  mkdir "D:\AI_Resources\_downloads"
if not exist "D:\AI_Resources\_tmp"        mkdir "D:\AI_Resources\_tmp"
echo   完成
echo.

REM ---- 用 curl 下载(GitHub release 在国内偶尔卡,GitHub 域名试一次)----
echo [2/8] 下载 Vosk SDK(14 MB)...
if exist "D:\AI_Resources\_downloads\vosk-sdk.zip" (
    echo   已存在,跳过下载
) else (
    curl -L -o "D:\AI_Resources\_downloads\vosk-sdk.zip" ^
         "https://github.com/alphacep/vosk-api/releases/download/v0.3.45/vosk-win64-0.3.45.zip"
    if errorlevel 1 (
        echo   下载失败!请检查网络,或手动从浏览器下载:
        echo   https://github.com/alphacep/vosk-api/releases/download/v0.3.45/vosk-win64-0.3.45.zip
        echo   保存为 D:\AI_Resources\_downloads\vosk-sdk.zip 后重跑本脚本
        pause
        exit /b 1
    )
)
echo.

echo [3/8] 下载 Piper 引擎(21 MB)...
if exist "D:\AI_Resources\_downloads\piper.zip" (
    echo   已存在,跳过下载
) else (
    curl -L -o "D:\AI_Resources\_downloads\piper.zip" ^
         "https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_windows_amd64.zip"
    if errorlevel 1 (
        echo   下载失败!请检查网络,或手动从浏览器下载:
        echo   https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_windows_amd64.zip
        echo   保存为 D:\AI_Resources\_downloads\piper.zip 后重跑本脚本
        pause
        exit /b 1
    )
)
echo.

REM ---- 用 PowerShell 解压(Expand-Archive)----
echo [4/8] 解压 Vosk SDK...
if exist "D:\AI_Resources\_downloads\vosk-extracted" rmdir /S /Q "D:\AI_Resources\_downloads\vosk-extracted"
powershell -NoProfile -Command "Expand-Archive -Path 'D:\AI_Resources\_downloads\vosk-sdk.zip' -DestinationPath 'D:\AI_Resources\_downloads\vosk-extracted' -Force"
if errorlevel 1 (
    echo   解压失败
    pause
    exit /b 1
)
echo   完成
echo.

echo [5/8] 把 libvosk.dll + vosk_api.h 拷到 vosk\sdk\...
copy /Y "D:\AI_Resources\_downloads\vosk-extracted\vosk-win64-0.3.45\libvosk.dll" "D:\AI_Resources\vosk\sdk\"
copy /Y "D:\AI_Resources\_downloads\vosk-extracted\vosk-win64-0.3.45\vosk_api.h"  "D:\AI_Resources\vosk\sdk\"
if errorlevel 1 (
    echo   拷贝失败
    pause
    exit /b 1
)
echo   完成
echo.

echo [6/8] 解压 Piper...
if exist "D:\AI_Resources\_downloads\piper-extracted" rmdir /S /Q "D:\AI_Resources\_downloads\piper-extracted"
powershell -NoProfile -Command "Expand-Archive -Path 'D:\AI_Resources\_downloads\piper.zip' -DestinationPath 'D:\AI_Resources\_downloads\piper-extracted' -Force"
if errorlevel 1 (
    echo   解压失败
    pause
    exit /b 1
)
echo   完成
echo.

echo [7/8] 把 piper 整个内容拷到 piper\bin\...
xcopy /E /I /Y "D:\AI_Resources\_downloads\piper-extracted\piper\*" "D:\AI_Resources\piper\bin\"
if errorlevel 1 (
    echo   拷贝失败
    pause
    exit /b 1
)
echo   完成
echo.

echo [8/8] 顺便拷贝 libvosk.dll 到 friend.exe 同目录(QLibrary 在此加载)...
if exist "d:\friend\build\friend.exe" (
    copy /Y "D:\AI_Resources\vosk\sdk\libvosk.dll" "d:\friend\build\"
    echo   完成
) else (
    echo   跳过:d:\friend\build\friend.exe 还不存在,请先编译
)
echo.

REM ---- 验证 ----
echo ========================================
echo   验证结果
echo ========================================
echo.
echo --- D:\AI_Resources\vosk\sdk\ 应有 libvosk.dll + vosk_api.h ---
dir /B "D:\AI_Resources\vosk\sdk\" 2>nul
echo.
echo --- D:\AI_Resources\piper\bin\ 应有 piper.exe 等 ---
dir /B "D:\AI_Resources\piper\bin\*.dll" "D:\AI_Resources\piper\bin\*.exe" 2>nul
echo.
echo --- D:\AI_Resources\piper\bin\espeak-ng-data\ 应存在 ---
if exist "D:\AI_Resources\piper\bin\espeak-ng-data\phondata" (
    echo   OK: espeak-ng-data 已就位
) else (
    echo   失败:espeak-ng-data 缺失
)
echo.

echo ========================================
echo   完成!还剩两件需手动做(国内网络限制)
echo ========================================
echo.
echo 1. Vosk 中文模型(alphacephei.com 国内打不开):
echo    浏览器开 https://alphacephei.com/vosk/models
echo    下载 vosk-model-small-cn-0.22.zip(42 MB)
echo    解压整个目录到 D:\AI_Resources\vosk\models\
echo    (最终路径应像:D:\AI_Resources\vosk\models\vosk-model-small-cn-0.22\conf\...)
echo.
echo 2. Piper 中文语音(huggingface.co 国内打不开):
echo    浏览器开 https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/zh/zh_CN/huayan/medium/zh_CN-huayan-medium.onnx
echo    下载并放到 D:\AI_Resources\piper\voices\
echo    再开 https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/zh/zh_CN/huayan/medium/zh_CN-huayan-medium.onnx.json
echo    下载并放到同目录(两个文件必须同名同目录)
echo.
echo 3. DeepSeek API key:
echo    注册 https://platform.deepseek.com 充值 1 元
echo    创建 API Key,填入 d:\friend\build\config.json 的 "llm.api_key"
echo.
echo 全部完成后,双击 d:\friend\build\friend.exe 即可使用!
echo.
pause

# 语音 AI 助手(Qt6 / C++,在线 LLM + 本地 ASR/TTS)

按住“🎤 按住说话”录音 → Vosk 语音识别 → 在线 LLM(DeepSeek/Groq)对话 → Piper 语音合成 → 播放。LLM 走在线 API(**不下载 4.7GB 本地模型**),ASR/TTS 仍本地离线。

## 技术栈

| 模块 | 方案 | 链接方式 |
|------|------|----------|
| 录音 | Qt6 `QAudioSource`(16k/mono/Int16) | Qt6 Multimedia |
| 语音识别 ASR | Vosk(中文模型) | **运行期 `QLibrary` 动态加载 `libvosk.dll`** |
| 对话大脑 LLM | 在线 API(DeepSeek/Groq,OpenAI 兼容) | HTTP(`QNetworkAccessManager`,SSE 流式) |
| 语音合成 TTS | Piper(`piper.exe`) | `QProcess` 子进程 |
| 播放 | Qt6 `QMediaPlayer`+`QAudioOutput` | Qt6 Multimedia |
| 界面 | Qt Widgets | Qt6 Widgets |

> Vosk 用动态加载,所以**没放 `libvosk.dll` 也能编译和启动**,只是 ASR 不可用、状态面板标红。

---

## 🥇 推荐:全 D 盘部署(不占 C 盘)

用户明确要求**下载和模型全部落到 D 盘**。本项目默认会自动探测并优先使用 **`D:\AI_Resources`** 作为资源根;也可在 `config.json` 里显式指定 `"resource_root": "D:/XXX"`(支持任意盘符路径)。

资源根生效后,app 启动时右侧状态面板最上方会显示实际生效路径 `"📁 资源根(资源优先放这里不占C盘)"`,并且**"打开资源/配置文件夹"按钮也会优先打开资源根**,方便你放文件。

### 目录约定(`D:\AI_Resources\*`)

```
D:\AI_Resources\
├── vosk\
│   ├── sdk\
│   │   ├── vosk_api.h
│   │   └── libvosk.dll      ← 从 vosk-win64-0.3.45.zip 取出
│   └── models\
│       └── vosk-model-small-cn-0.22\   ← 解压后的模型目录
└── piper\
    ├── bin\
    │   ├── piper.exe
    │   ├── onnxruntime.dll
    │   ├── espeak-ng.dll
    │   └── espeak-ng-data\              ← 随 piper_windows_amd64.zip
    └── voices\
        ├── zh_CN-huayan-medium.onnx     ← 中文语音
        └── zh_CN-huayan-medium.onnx.json
```

### LLM API key(无需下载大模型,天然不占 C 盘)

LLM 已改为在线 API,**不再需要下载 4.7GB 的本地模型**,所以这一步不会占任何磁盘。二选一(都兼容 OpenAI 接口):

**DeepSeek(推荐:国内直连无需代理,便宜)**
1. 打开 https://platform.deepseek.com 注册登录
2. 充值(最少 1 元即可,`deepseek-chat` 约 0.001 元/千 token,日常对话几分钱用很久)
3. 左侧 “API Keys” → 创建 API Key,复制
4. 填入 `d:\friend\config.json` 的 `llm` 段:
   ```json
   "llm": {
     "base_url": "https://api.deepseek.com/v1",
     "api_key": "sk-你的key",
     "model": "deepseek-chat"
   }
   ```

**Groq(有免费额度,但国内需代理访问)**
1. 打开 https://console.groq.com 注册(可 Google 登录)
2. 左侧 “API Keys” → Create API Key,复制
3. 填入:
   ```json
   "llm": {
     "base_url": "https://api.groq.com/openai/v1",
     "api_key": "gsk_你的key",
     "model": "llama-3.3-70b-versatile"
   }
   ```

> Groq 在国内需走代理。设系统代理,或在启动 `friend.exe` 前的终端设环境变量:
> `$env:HTTPS_PROXY="http://127.0.0.1:7890"`(端口换成你代理软件的)。DeepSeek 国内直连,无需此步。

### 浏览器下载时把压缩包直接存 D 盘

避免文件先下到 `C:\Users\...\Downloads` 再复制(浪费时间 + 多占一份空间):

- Vosk SDK `vosk-win64-0.3.45.zip` / Vosk 模型 `vosk-model-small-cn-0.22.zip` → 另存为 `D:\AI_Resources\_downloads\`(自己建个 _downloads 子目录)
- Piper 引擎 `piper_windows_amd64.zip` / 语音 `zh_CN-huayan-medium*` → 也存 `D:\AI_Resources\_downloads\`
- 直接在 D 盘解压到上面对应的 `vosk/sdk`、`vosk/models/*`、`piper/bin`、`piper/voices` 目录,**解压完成后 zip 可以删掉**

### (可选)显式声明资源根

如果不喜欢默认的 `D:\AI_Resources`,改 `d:\friend\config.json` 里的 `"resource_root"`:

```json
{
  "resource_root": "D:/MyAI",
  "vosk":   { "model_dir": "", "dll": "" },
  "piper":  { "exe": "", "voice": "", "length_scale": 1.0 },
  "llm":    { "base_url": "https://api.deepseek.com/v1", "api_key": "", "model": "deepseek-chat" },
  "audio":  { "sample_rate": 16000 }
}
```

留空 = 自动按 `D:\AI_Resources` → 项目根的回退顺序探测。

---

## 一、准备依赖(三选三,都要做)

> 本节仅保留项目根内备选目录(资源根没找到时才用)。**优先按上一节 D 盘方案放**。

### 1. Vosk(ASR)
- 下载 SDK:https://github.com/alphacep/vosk-api/releases 取 `vosk-win64-0.3.45+` 解压,把
  - `vosk_api.h`、`libvosk.dll` 放到 `D:\AI_Resources\vosk\sdk\`(或回退:项目根 `third_party\vosk\`)
- 下载中文模型:https://alphacephei.com/vosk/models
  - 起步推荐 `vosk-model-small-cn-0.22.zip`(~42MB),解压到 `D:\AI_Resources\vosk\models\vosk-model-small-cn-0.22\`(或回退:项目根 `models\vosk\`)
  - (精度高可用 `vosk-model-cn-0.22.zip` ~1.3GB)
- 部署时把 `libvosk.dll` 复制到 exe 同目录(见下文“运行”)。

### 2. Piper(TTS)
- 下载引擎:https://github.com/rhasspy/piper/releases 取 `piper_windows_amd64.zip`(release `2023.11.14-2`)
  - 解压到 `D:\AI_Resources\piper\bin\`,确保含 `piper.exe`、`onnxruntime.dll`、`espeak-ng.dll`、`espeak-ng-data\`(或回退:项目根 `runtime\piper\`)
- 下载中文语音(两个文件同目录):
  - `zh_CN-huayan-medium.onnx`
  - `zh_CN-huayan-medium.onnx.json`
  - 地址:https://huggingface.co/rhasspy/piper-voices/tree/v1.0.0/zh/zh_CN/huayan/medium
  - 放到 `D:\AI_Resources\piper\voices\`(或回退:项目根 `runtime\piper\voices\`)

### 3. LLM API key(无需下载)
按上方 “🥇 推荐” 专节的 **LLM API key** 小节,申请 DeepSeek 或 Groq 的 key 填入 `config.json` 的 `llm` 段。**无需下载任何大模型文件**,不占磁盘。

## 二、配置(可选)

默认 `config.json` 字段留空 = 自动探测,绝大多数情况无需改动。仅当路径不在默认位置时填写(支持绝对路径或相对项目根的相对路径)。**支持显式 `resource_root` 把大文件统一放到任一盘符**(见 D 盘部署专节):

```json
{
  "resource_root": "",
  "vosk":   { "model_dir": "", "dll": "" },
  "piper":  { "exe": "", "voice": "", "length_scale": 1.0 },
  "llm":    { "base_url": "https://api.deepseek.com/v1", "api_key": "", "model": "deepseek-chat" },
  "audio":  { "sample_rate": 16000 }
}
```

## 三、构建(修复 PowerShell 执行策略后)

```powershell
# 0. 一次性修复执行策略(管理员或当前用户均可)
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned

# 1. 开发终端设 PATH(每开一个新终端执行一次)
$env:PATH = "D:\QT\6.8.3\mingw_64\bin;D:\QT\Tools\mingw1310_64\bin;D:\QT\Tools\CMake_64\bin;D:\QT\Tools\Ninja;" + $env:PATH

# 2. 配置
cmake -S d:\friend -B d:\friend\build -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH=D:/QT/6.8.3/mingw_64 `
  -DCMAKE_C_COMPILER=D:/QT/Tools/mingw1310_64/bin/gcc.exe `
  -DCMAKE_CXX_COMPILER=D:/QT/Tools/mingw1310_64/bin/g++.exe

# 3. 构建
cmake --build d:\friend\build --parallel
```

## 四、运行

```powershell
# 4. 部署 Qt 运行时到 exe 同目录
D:\QT\6.8.3\mingw_64\bin\windeployqt --debug d:\friend\build\friend.exe

# 5. 复制 libvosk.dll 到 exe 同目录(QLibrary 在此加载)
#    优先从 D:\AI_Resources\vosk\sdk 取,否则从项目内 fallback 取
if (Test-Path "D:\AI_Resources\vosk\sdk\libvosk.dll") {
  copy "D:\AI_Resources\vosk\sdk\libvosk.dll" d:\friend\build\
} elseif (Test-Path "d:\friend\third_party\vosk\libvosk.dll") {
  copy d:\friend\third_party\vosk\libvosk.dll d:\friend\build\
}

# 6. 启动(在 d:\friend 下运行,保证相对路径与模型探测正确)
d:\friend\build\friend.exe
```

## 五、冒烟测试

1. **空配置启动**:不下载任何模型直接运行 → 窗口能打开,右侧“依赖状态”三项标红,左下提示可用输入框手敲。
2. **录音**:配置 Vosk 后,按住“🎤 按住说话”,状态栏实时显示“听到:…”,松开出最终中文文本并自动发问。
3. **LLM**:`config.json` 填好 `api_key` → 看到流式回复逐字出现,面板 LLM 显绿“在线(N 个模型可用)”。
4. **TTS**:配置 Piper 后,回复完成自动朗读中文语音。
5. **降级**:`api_key` 留空启动 → 仍能启动,面板 LLM 显红“未填 api_key”;ASR/TTS 缺失同样标红但程序照常运行。

## 目录结构(优先 D 盘资源根)

**主资源目录**:
```
D:\AI_Resources\         # (默认自动探测;可用 config.json > resource_root 显式改)
├── vosk\sdk\libvosk.dll
├── vosk\models\vosk-model-small-cn-0.22\
├── piper\bin\piper.exe  (+ onnxruntime.dll, espeak-ng-data\)
├── piper\voices\zh_CN-huayan-medium.onnx (+ .onnx.json)
├── _downloads\          # 自己建的临时下载目录,解压完可删 zip
└── _tmp\                # 程序运行时临时音频(替代 C 盘 %TEMP%,不占 C 盘)
```

**项目目录**(只有源码+配置,体积小,继续在 `d:\friend`):
```
d:\friend\
├── CMakeLists.txt
├── config.json
├── third_party\vosk\    # Vosk fallback (不推荐用,建议用 D:\AI_Resources)
├── models\vosk\         # Vosk fallback
├── runtime\piper\       # Piper fallback
├── src\                 # 源码(本项目)
└── build\               # 构建产物(生成)
```

## 常见问题

- **`未找到 Qt6`**:CMake 时漏了 `-DCMAKE_PREFIX_PATH=D:/QT/6.8.3/mingw_64`。
- **运行闪退/找不到 dll**:没跑 `windeployqt`,或没把 `libvosk.dll` 拷到 exe 旁。
- **按说话没反应**:Vosk 模型没放,看右侧面板 ASR 是否标红。
- **LLM 报错 `未配置 API key`**:`config.json` 的 `llm.api_key` 没填。
- **LLM 报错 `Host not found` / `connection refused`**:Groq 需代理(设 `HTTPS_PROXY`);DeepSeek 应国内直连。
- **中文语音合成报错**:Piper 的 `.onnx` 与 `.onnx.json` 必须同名同目录。

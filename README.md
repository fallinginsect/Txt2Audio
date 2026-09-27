# Txt2Audio - 本地TXT转音频听书工具
基于 Qt 6 + MeloTTS 的本地听书工具，将 TXT 文本合成为 WAV 音频。

## 功能
- 拖拽或点击选择 TXT 文件
- 自动按句子边界分段（≤4000字符）
- 调用本地 MeloTTS 服务合成音频
- 自动合并分段为完整 WAV 文件

## 发行版用法
前往 [Releases](链接) 下载最新版 ZIP，解压后双击 `Txt2Audio.exe` 即可运行。

> **注意**：使用前需先启动本地 MeloTTS 服务端（见下文部署步骤），否则会提示网络请求失败。

## 环境要求
- Qt 6.10.2 (MinGW 64-bit) — 仅开发者编译客户端需要
- Anaconda / Miniconda
- Python 3.10

## 服务端部署
### 1. 创建虚拟环境
```bash
conda create -n melotts python=3.10
conda activate melotts
```

### 2. 安装 PyTorch

根据你的硬件选择对应版本：

#### GPU 版本（CUDA 12.1）

```
pip install torch==2.5.1 torchaudio==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cu121
```

#### CPU 版本（无 GPU）

```
pip install torch==2.5.1 torchaudio==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cpu
```

### 3. 降级 numpy

PyTorch 安装时会自动拉取 numpy 2.x，与 `librosa==0.9.1` 不兼容，必须降级到 1.26.4：

```
pip install "numpy==1.26.4" --force-reinstall
```

### 4. 配置清华镜像源

加速后续依赖下载：

```
pip config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple
pip config set global.trusted-host pypi.tuna.tsinghua.edu.cn
```

### 5. 安装项目依赖

在项目根目录下执行：

```
cd server 
pip install -r requirements.txt
```

> 
> **说明**：`requirements.txt` 中已锁定 `setuptools==79.0.1`，用于兼容 librosa 0.9.1 所需的 `pkg_resources` 模块（setuptools 81+ 版本已移除该模块）。

### 6. 安装 MeloTTS

必须加 `--no-deps` 参数，否则会重新拉取 torch、numpy 等依赖，覆盖前面调好的版本：

```
pip install git+https://github.com/myshell-ai/MeloTTS.git@209145371cff8fc3bd60d7be902ea69cbdb7965a --no-deps
```

### 7. 下载模型

可使用国内镜像站加速下载：

```
set HF_ENDPOINT=https://hf-mirror.com
huggingface-cli download myshell-ai/MeloTTS-Chinese --local-dir C:\Users\93095\MeloTTS\openvoice\basespeakers\ZH
```

### 8. 日语分词适配（unidic-lite 方案）

原版 unidic 词典国内下载困难，替换为 lite 版本：

1. 找到 `melo/text/japanese.py` 文件
2. 在文件开头插入：

```
import os
import unidic_lite
```

3. 找到第 368 行附近的代码：

```
_TAGGER = MeCab.Tagger()
```

替换为：

```
dic_dir = unidic_lite.DICDIR
_TAGGER = MeCab.Tagger(f'-r "{os.path.join(dic_dir, "mecabrc")}" -d "{dic_dir}"')
```

### 9. 关于 NLTK 数据包

英文文本处理依赖 NLTK 数据包，程序首次启动时会自动从 GitHub 下载。

> 
> **注意**：不建议手动下载文件放到目录，可能出现程序检测不到的情况，首次启动请确保网络能正常访问 GitHub。

### 10. 启动服务

执行前请确保已激活 melotts 虚拟环境：

```
cd server
start_melo.bat
```

### 11. 验证环境

执行以下两条命令，确认环境正常：

```
python -c "import torch, numpy, librosa, transformers, gradio; print('OK')"
pip check
```

- 第一条输出 `OK`
- 第二条输出 `No broken requirements found.`（少量无关警告可忽略）

## 客户端编译（下载源码适用）

1. 用 Qt Creator 打开 `client` 目录
2. 选择构建套件：`Desktop Qt 6.10.2 MinGW 64-bit`
3. 切换到 Release 模式，重新构建项目即可

## 使用说明

1. 先启动 MeloTTS 服务器，确认服务正常运行
2. 打开客户端，导入 `.txt` 文件
3. 选择输出目录和语速
4. 点击开始合成

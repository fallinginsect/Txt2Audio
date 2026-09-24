# Txt2Audio - 本地TXT转音频听书工具
基于 Qt 6 + MeloTTS 的本地听书工具，将 TXT 文本合成为 WAV 音频。

## 功能
拖拽或点击选择 TXT 文件
自动按句子边界分段（≤4000字符）
调用本地 MeloTTS 服务合成音频
自动合并分段为完整 WAV 文件

## 发行版用法
前往 [Releases](链接) 下载最新版 ZIP，解压后双击 `Txt2Audio.exe`。
**注意**：使用前需先启动本地 MeloTTS 服务端（见下方）。否则会网络请求失败

## 环境要求
- Qt 6.10.2 (MinGW 64-bit) — 仅开发者编译需要
- Anaconda / Miniconda
- Python 3.10

## 服务端配置
```bash
conda create -n melotts python=3.10
conda activate melotts
# GPU 版本（CUDA 12.1，有其他版本需求自己更改）
pip install torch==2.5.1 torchaudio==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cu121

# CPU 版本（无 GPU）
pip install torch==2.5.1 torchaudio==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cpu

### 降级 numpy

# PyTorch 安装时会自动拉 numpy 2.x，和 `librosa==0.9.1` 不兼容，必须降版本到1.26.4
pip install "numpy==1.26.4" --force-reinstall

##配置清华源
pip config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple
pip config set global.trusted-host pypi.tuna.tsinghua.edu.cn

## 安装项目依赖
## 如果你在项目目录下
cd server 
pip install -r requirements.txt

#requirements.txt 中已包含 setuptools==79.0.1，用于兼容 librosa 0.9.1 需要的 pkg_resources 模块（setuptools 81+ 已移除该模块）。

# 下载MeloTTS, 必须加 --no-deps，否则它会重新拉取 torch、numpy 等依赖，覆盖前面装好的版本：
pip install git+https://github.com/myshell-ai/MeloTTS.git@209145371cff8fc3bd60d7be902ea69cbdb7965a --no-deps

## 下载模型，可以设置国内镜像站
set HF_ENDPOINT=https://hf-mirror.com
huggingface-cli download myshell-ai/MeloTTS-Chinese --local-dir C:\Users\93095\MeloTTS\openvoice\basespeakers\ZH

## 最后，由于unidic本体国内下载有问题，我们用的是-lite版本
## melotts的 japanese.py里面用的是unidic
# 打开melo/text/japanese.py
# 在前面插入
import os
import unidic_lite

*把368行附近的代码*
_TAGGER = MeCab.Tagger()
*换成*
dic_dir = unidic_lite.DICDIR
_TAGGER = MeCab.Tagger(f'-r "{os.path.join(dic_dir, "mecabrc")}" -d "{dic_dir}"')


## 启动服务，注意先激活环境哦
# 运行
cd server
start_melo.bat

##以及一个nltk插件，我们需要额外下载数据包，会自动上github下载，注意这次下载要让程序来做
##自己下载相关文件并且放在对应目录里似乎程序检测不到
##所以首次启动记得确保能连上github服务

## 验证环境
python -c "import torch, numpy, librosa, transformers, gradio; print('OK')"
pip check
#第一条输出 OK
#第二条输出 No broken requirements found.（或只有少量无关警告）


## 客户端编译(下载源码的话)
1. 用 Qt creator打开 client
2. 选择kit: Desktop Qt 6.10.2 WinGW 64-bit

## 使用
1. 启动MeloTTs 服务器(注意:一定要先打开服务器，确保服务器正常运行，客户端才能正常运行)
2. 打开客户端，导入.txt文件
3. 选择输出目录和语速
4. 开始合成
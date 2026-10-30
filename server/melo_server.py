import sys
from unittest.mock import MagicMock

# 在导入 MeloTTS 之前，用一个“假模块”顶替掉日语模块
# 这样 MeloTTS 内部无论怎么 import 和初始化，拿到的都是一个不会联网、不会报错的空壳
sys.modules['melo.text.japanese'] = MagicMock()
sys.modules['melo.text.korean'] = MagicMock()
sys.modules['melo.text.french'] = MagicMock()
sys.modules['melo.text.spanish'] = MagicMock()

import uvicorn
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from melo.api import TTS
import tempfile
import os
import base64
import numpy as np
import soundfile as sf
from fastapi.concurrency import run_in_threadpool
import wave
import traceback

app = FastAPI()
is_cancelled = False

TTS_LANGUAGE = "ZH"

# 全局加载模型（ZH 支持中英混读）
print("正在加载 MeloTTS 模型，请稍候...")
model = TTS(language=TTS_LANGUAGE)
speaker_ids = model.hps.data.spk2id
print("模型加载成功！")
print(speaker_ids.items())

class TTSRequest(BaseModel):
    text: str
    speaker_id: int = 1
    speed: float = 1.0

@app.get("/speakers")#拉取对应语言模型有的speaker列表
async def get_speakers():
    speaker_list = []
    for name, spk_id in speaker_ids.items():
        speaker_list.append({
            "id": spk_id,
            "name": name
        })
    return {"speakers": speaker_list}


def normalize_loudness(wav_path, target_db=-20.0):
    """对 WAV 做整体响度归一化"""
    data, sr = sf.read(wav_path, dtype='float32')
    
    # 计算 RMS
    if data.ndim > 1:
        rms = np.sqrt(np.mean(data ** 2, axis=0)).mean()
    else:
        rms = np.sqrt(np.mean(data ** 2))
    
    if rms < 1e-6:
        return  # 静音，不动
    
    current_db = 20 * np.log10(rms)
    gain = 10 ** ((target_db - current_db) / 20)
    
    data = data * gain
    
    # 防止削波
    peak = np.max(np.abs(data))
    if peak > 0.99:
        data = data * (0.99 / peak)
    
    sf.write(wav_path, data, sr)

@app.post("/reset")
async def reset_task():
    global is_cancelled
    is_cancelled = False
    return {"status": "reset"}

@app.post("/cancel")
async def cancel_task():
    global is_cancelled
    is_cancelled = True
    return {"status": "cancelled"}


@app.post("/synthesize")
async def synthesize(req: TTSRequest):
    global is_cancelled
    
    # 1. 进来先查状态
    if is_cancelled:
        raise HTTPException(status_code=400, detail="任务已取消")

    if not req.text.strip():
        raise HTTPException(status_code=400, detail="文本不能为空")
    if len(req.text) > 4000:
        raise HTTPException(status_code=413, detail="文本超过4000字限制")
    
    text = req.text.strip().replace('_', ' ')
    
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
        tmp_path = f.name

    try:
        # 2. 把阻塞的模型合成丢进线程池，主线程抽身出来接 /cancel
        await run_in_threadpool(
            model.tts_to_file, text, req.speaker_id, tmp_path, speed=req.speed
        )

        # 3. 合成完毕，再看看是不是中途被取消了
        if is_cancelled:
            raise HTTPException(status_code=400, detail="任务已取消")

        normalize_loudness(tmp_path, target_db=-20.0)
        
        with open(tmp_path, "rb") as audio_file:#把模型生成的音频读出来
            audio_bytes = audio_file.read()
        return {"audio": base64.b64encode(audio_bytes).decode("utf-8")}
    except HTTPException as h:
        raise h # 防止我们这里要报400的网络异常被捕获
    except Exception as e:
        # 打印错误并抛出异常
        print(f"[ERROR] 大模型合成发生错误: {e}")
        traceback.print_exc() 
        # FastAPI 会自动捕获这个异常，给客户端返回 500 Internal Server Error
        raise HTTPException(status_code=500, detail=f"合成崩溃: {str(e)}")
        
    finally:
        if os.path.exists(tmp_path):#删除tmp_path,因为任务结束了
            os.unlink(tmp_path)

@app.get("/health")
async def health():
    return {"status": "ok"}

if __name__ == "__main__":
    '''运行这个文件的时候才启动服务'''
    uvicorn.run(app, host="127.0.0.1", port=8000)

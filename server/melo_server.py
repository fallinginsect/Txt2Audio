import uvicorn
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from melo.api import TTS
import tempfile
import os
import base64
import numpy as np
import soundfile as sf

app = FastAPI()

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



@app.post("/synthesize")
async def synthesize(req: TTSRequest):
    if not req.text.strip():
        raise HTTPException(status_code=400, detail="文本不能为空")
    if len(req.text) > 4000:
        raise HTTPException(status_code=413, detail="文本超过4000字限制")
    text = req.text.strip().replace('_', ' ')   
    
    # 生成临时 WAV 文件
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
        tmp_path = f.name
    
    # 调用 MeloTTS 生成音频
    model.tts_to_file(text, req.speaker_id, tmp_path, speed=req.speed)
    normalize_loudness(tmp_path, target_db=-20.0)
    
    # 读取并转 Base64 返回
    with open(tmp_path, "rb") as audio_file:
        audio_bytes = audio_file.read()
    os.unlink(tmp_path)  # 删除临时文件
    
    return {"audio": base64.b64encode(audio_bytes).decode("utf-8")}

@app.get("/health")
async def health():
    return {"status": "ok"}

if __name__ == "__main__":
    '''运行这个文件的时候才启动服务'''
    uvicorn.run(app, host="127.0.0.1", port=8000)

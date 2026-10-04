@echo off
call conda activate melotts
set TRANSFORMERS_OFFLINE=1
set HF_HUB_OFFLINE=1
python melo_server.py
pause
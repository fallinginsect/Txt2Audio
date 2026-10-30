@echo off
call conda activate melotts
set TRANSFORMERS_OFFLINE=0
set HF_HUB_OFFLINE=0
python melo_server.py
pause
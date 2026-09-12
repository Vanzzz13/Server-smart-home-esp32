import os
import re
from fastapi import FastAPI, UploadFile, File, Header, HTTPException
from fastapi.responses import JSONResponse
from groq import Groq

app = FastAPI(title="ESP32 Smart Home Voice API", version="1.0.0")

GROQ_API_KEY = os.getenv("GROQ_API_KEY")
DEVICE_TOKEN = os.getenv("DEVICE_TOKEN")

if not GROQ_API_KEY:
    raise RuntimeError("GROQ_API_KEY belum diatur")
if not DEVICE_TOKEN:
    raise RuntimeError("DEVICE_TOKEN belum diatur")

client = Groq(api_key=GROQ_API_KEY)

def check_token(authorization: str | None):
    if authorization != f"Bearer {DEVICE_TOKEN}":
        raise HTTPException(status_code=403, detail="Invalid device token")

def parse_command(text: str):
    text = text.lower().strip()
    replacements = {
        "hidupkan": "nyalakan",
        "hidupkanlah": "nyalakan",
        "nyalakanlah": "nyalakan",
        "matikanlah": "matikan",
        "output satu": "output 1",
        "output dua": "output 2",
        "output tiga": "output 3",
        "output empat": "output 4",
        "relay satu": "relay 1",
        "relay dua": "relay 2",
        "relay tiga": "relay 3",
        "relay empat": "relay 4",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)

    m = re.search(r"(?:nyalakan|hidupkan).*?(?:output|relay)\s*([1-4])", text)
    if m:
        return {"success": True, "command": "relay", "output": int(m.group(1)),
                "state": "on", "text": text}

    m = re.search(r"matikan.*?(?:output|relay)\s*([1-4])", text)
    if m:
        return {"success": True, "command": "relay", "output": int(m.group(1)),
                "state": "off", "text": text}

    return {"success": False, "command": "unknown", "output": 0,
            "state": "unknown", "text": text}

@app.get("/")
def root():
    return {"status": "online", "service": "ESP32 Smart Home Voice API"}

@app.get("/health")
def health():
    return {"status": "ok"}

@app.post("/api/voice")
async def voice_command(
    audio: UploadFile = File(...),
    authorization: str | None = Header(default=None)
):
    check_token(authorization)

    if audio.content_type not in ("audio/wav", "audio/x-wav", "application/octet-stream"):
        raise HTTPException(status_code=400, detail="Audio harus WAV")

    audio_data = await audio.read()
    if not audio_data or len(audio_data) > 5 * 1024 * 1024:
        raise HTTPException(status_code=413, detail="Audio kosong atau terlalu besar")

    try:
        transcription = client.audio.transcriptions.create(
            file=("voice.wav", audio_data, "audio/wav"),
            model="whisper-large-v3-turbo",
            language="id",
            prompt=(
                "Perintah smart home bahasa Indonesia. "
                "Contoh: nyalakan output 1, matikan output 1, "
                "nyalakan output 2, matikan output 2, "
                "nyalakan output 3, matikan output 3, "
                "nyalakan output 4, matikan output 4."
            ),
            temperature=0,
            response_format="json",
        )
        text = transcription.text
    except Exception as exc:
        print("STT ERROR:", repr(exc))
        raise HTTPException(status_code=502, detail="Speech-to-text gagal")

    result = parse_command(text)
    print("VOICE:", text, "=>", result)
    return JSONResponse(content=result)

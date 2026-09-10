import os
import re
import requests

from fastapi import FastAPI, UploadFile, File, Header, HTTPException

app = FastAPI(title="ESP32 Smart Home Voice API", version="1.0.0")

GROQ_API_KEY = os.getenv("GROQ_API_KEY")
DEVICE_TOKEN = os.getenv("DEVICE_TOKEN")

GROQ_URL = "https://api.groq.com/openai/v1/audio/transcriptions"
WHISPER_MODEL = "whisper-large-v3-turbo"


@app.get("/")
def root():
    return {
        "status": "online",
        "service": "ESP32 Smart Home Voice API",
        "stt": "Groq Whisper"
    }


@app.get("/health")
def health():
    return {"status": "ok"}


def check_device_token(authorization: str | None):
    if not DEVICE_TOKEN:
        raise HTTPException(status_code=500, detail="DEVICE_TOKEN belum dikonfigurasi")

    if authorization != f"Bearer {DEVICE_TOKEN}":
        raise HTTPException(status_code=401, detail="Invalid device token")


def normalize_text(text: str):
    text = text.lower().strip()

    replacements = {
        "satu": "1",
        "dua": "2",
        "tiga": "3",
        "empat": "4",
        "pertama": "1",
        "kedua": "2",
        "ketiga": "3",
        "keempat": "4",
    }

    for old, new in replacements.items():
        text = text.replace(old, new)

    return text


def parse_command(text: str):
    text = normalize_text(text)

    if re.search(r"\b(nyalakan|hidupkan|aktifkan)\b", text):
        state = "on"
    elif re.search(r"\b(matikan|nonaktifkan)\b", text):
        state = "off"
    else:
        return None

    match = re.search(r"\b(?:output|relay)\s*([1-4])\b", text)
    if not match:
        return None

    return {
        "command": "relay",
        "output": int(match.group(1)),
        "state": state,
        "text": text,
    }


@app.post("/api/voice")
async def voice(
    audio: UploadFile = File(...),
    authorization: str | None = Header(default=None),
):
    check_device_token(authorization)

    if not GROQ_API_KEY:
        raise HTTPException(status_code=500, detail="GROQ_API_KEY belum dikonfigurasi")

    audio_data = await audio.read()
    if not audio_data:
        raise HTTPException(status_code=400, detail="Audio kosong")

    filename = audio.filename or "audio.wav"

    headers = {"Authorization": f"Bearer {GROQ_API_KEY}"}
    files = {"file": (filename, audio_data, "audio/wav")}
    data = {
        "model": WHISPER_MODEL,
        "language": "id",
        "response_format": "json",
        "temperature": "0",
        "prompt": (
            "Perintah smart home bahasa Indonesia. Kemungkinan perintah: "
            "nyalakan output 1, matikan output 1, "
            "nyalakan output 2, matikan output 2, "
            "nyalakan output 3, matikan output 3, "
            "nyalakan output 4, matikan output 4."
        ),
    }

    try:
        response = requests.post(
            GROQ_URL,
            headers=headers,
            files=files,
            data=data,
            timeout=60,
        )
    except requests.RequestException as exc:
        raise HTTPException(status_code=502, detail=f"Groq request error: {exc}")

    if response.status_code != 200:
        raise HTTPException(status_code=502, detail=f"Groq error: {response.text}")

    result = response.json()
    text = result.get("text", "").strip()

    if not text:
        return {"success": False, "error": "Suara tidak dikenali", "text": ""}

    command = parse_command(text)
    if command is None:
        return {"success": False, "error": "Perintah tidak dikenali", "text": text}

    return {"success": True, **command}

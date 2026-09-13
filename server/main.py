import os
import re
import speech_recognition as sr
from fastapi import FastAPI, UploadFile, File, Header, HTTPException
from fastapi.responses import JSONResponse

app = FastAPI(title="ESP32 Smart Home Voice API", version="2.0.0")
DEVICE_TOKEN = os.getenv("DEVICE_TOKEN")

if not DEVICE_TOKEN:
    raise RuntimeError("DEVICE_TOKEN belum diatur")

recognizer = sr.Recognizer()

def check_token(authorization: str | None):
    if authorization != f"Bearer {DEVICE_TOKEN}":
        raise HTTPException(status_code=403, detail="Invalid device token")

def parse_command(original: str):
    text = re.sub(r"[,!?;:.]+", " ", original.lower().strip())
    text = re.sub(r"\s+", " ", text).strip()

    # Wake word wajib. Toleransi terhadap beberapa hasil STT yang umum.
    wake = re.search(r"\b(evan|even|evann)\b", text)
    if not wake:
        return {
            "success": False, "command": "unknown", "output": 0,
            "state": "unknown", "text": original,
            "reason": "wake_word_missing"
        }

    s = text[wake.end():].strip()
    replacements = {
        "hidupkan": "nyalakan",
        "aktifkan": "nyalakan",
        "nyalakanlah": "nyalakan",
        "matikanlah": "matikan",
        "satu": "1",
        "dua": "2",
        "tiga": "3",
        "empat": "4",
    }
    for old, new in replacements.items():
        s = re.sub(rf"\b{re.escape(old)}\b", new, s)

    m = re.search(r"\b(?:output|relay)\s*([1-4])\b", s)
    if not m:
        return {
            "success": False, "command": "unknown", "output": 0,
            "state": "unknown", "text": original,
            "reason": "output_not_found"
        }

    output = int(m.group(1))
    if re.search(r"\bnyalakan\b", s):
        state = "on"
    elif re.search(r"\bmatikan\b", s):
        state = "off"
    else:
        return {
            "success": False, "command": "unknown", "output": 0,
            "state": "unknown", "text": original,
            "reason": "action_not_found"
        }

    return {
        "success": True, "command": "relay", "output": output,
        "state": state, "text": original
    }

@app.get("/")
def root():
    return {"status": "online", "service": "Evan Smart Home Voice API", "stt": "Google Speech Recognition"}

@app.get("/health")
def health():
    return {"status": "ok", "stt": "google"}

@app.post("/api/voice")
async def voice_command(
    audio: UploadFile = File(...),
    authorization: str | None = Header(default=None)
):
    check_token(authorization)

    if audio.content_type not in (
        "audio/wav", "audio/x-wav", "application/octet-stream"
    ):
        raise HTTPException(status_code=400, detail="Audio harus WAV")

    audio_data = await audio.read()
    if not audio_data or len(audio_data) > 5 * 1024 * 1024:
        raise HTTPException(status_code=413, detail="Audio kosong atau terlalu besar")

    # speech_recognition membaca WAV langsung dari file sementara.
    temp_path = "/tmp/evan_voice.wav"
    try:
        with open(temp_path, "wb") as f:
            f.write(audio_data)

        with sr.AudioFile(temp_path) as source:
            recorded = recognizer.record(source)

        # Google Speech Recognition dipakai sebagai STT alternatif.
        text = recognizer.recognize_google(recorded, language="id-ID")
    except sr.UnknownValueError:
        return JSONResponse(content={
            "success": False, "command": "unknown", "output": 0,
            "state": "unknown", "text": "",
            "reason": "speech_not_understood"
        })
    except sr.RequestError as exc:
        print("STT ERROR:", repr(exc))
        raise HTTPException(status_code=502, detail="Layanan Speech-to-Text tidak dapat diakses")
    except Exception as exc:
        print("AUDIO/STT ERROR:", repr(exc))
        raise HTTPException(status_code=502, detail="Speech-to-text gagal")
    finally:
        try:
            os.remove(temp_path)
        except OSError:
            pass

    result = parse_command(text)
    print("VOICE:", text, "=>", result)
    return JSONResponse(content=result)

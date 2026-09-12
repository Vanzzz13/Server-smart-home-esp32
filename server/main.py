import os, re
from fastapi import FastAPI, UploadFile, File, Header, HTTPException
from fastapi.responses import JSONResponse
from groq import Groq

app = FastAPI(title="Evan Smart Home Voice API", version="1.1.0")
GROQ_API_KEY = os.getenv("GROQ_API_KEY")
DEVICE_TOKEN = os.getenv("DEVICE_TOKEN")
if not GROQ_API_KEY: raise RuntimeError("GROQ_API_KEY belum diatur")
if not DEVICE_TOKEN: raise RuntimeError("DEVICE_TOKEN belum diatur")
client = Groq(api_key=GROQ_API_KEY)

def check_token(authorization: str | None):
    if authorization != f"Bearer {DEVICE_TOKEN}":
        raise HTTPException(status_code=403, detail="Invalid device token")

def parse_command(text: str):
    original = text
    text = re.sub(r"[,!?;:.]+", " ", text.lower().strip())
    text = re.sub(r"\s+", " ", text).strip()

    # Wake word wajib. Toleransi kecil untuk hasil STT yang umum.
    wake = re.search(r"\b(evan|even|evann|evanv)\b", text)
    if not wake:
        return {"success":False,"command":"unknown","output":0,"state":"unknown",
                "text":original,"reason":"wake_word_missing"}

    s = text[wake.end():].strip()
    s = s.replace("hidupkan", "nyalakan").replace("aktifkan", "nyalakan")
    s = s.replace("nyalakanlah", "nyalakan").replace("matikanlah", "matikan")
    nums = {"satu":"1","dua":"2","tiga":"3","empat":"4"}
    for word, digit in nums.items():
        s = re.sub(rf"\b{word}\b", digit, s)

    m = re.search(r"\b(?:output|relay)\s*([1-4])\b", s)
    if not m:
        return {"success":False,"command":"unknown","output":0,"state":"unknown",
                "text":original,"reason":"output_not_found"}
    output = int(m.group(1))
    if re.search(r"\bnyalakan\b", s): state = "on"
    elif re.search(r"\bmatikan\b", s): state = "off"
    else:
        return {"success":False,"command":"unknown","output":0,"state":"unknown",
                "text":original,"reason":"action_not_found"}
    return {"success":True,"command":"relay","output":output,"state":state,"text":original}

@app.get("/")
def root(): return {"status":"online","service":"Evan Smart Home Voice API"}

@app.get("/health")
def health(): return {"status":"ok"}

@app.post("/api/voice")
async def voice_command(audio: UploadFile = File(...),
                        authorization: str | None = Header(default=None)):
    check_token(authorization)
    if audio.content_type not in ("audio/wav","audio/x-wav","application/octet-stream"):
        raise HTTPException(status_code=400, detail="Audio harus WAV")
    audio_data = await audio.read()
    if not audio_data or len(audio_data) > 5*1024*1024:
        raise HTTPException(status_code=413, detail="Audio kosong atau terlalu besar")
    try:
        t = client.audio.transcriptions.create(
            file=("voice.wav", audio_data, "audio/wav"),
            model="whisper-large-v3-turbo",
            language="id",
            prompt=("Perintah smart home bahasa Indonesia. Wake word selalu Evan. "
                    "Contoh: Evan, nyalakan output satu. Evan, matikan output satu. "
                    "Evan, nyalakan output dua. Evan, matikan output dua. "
                    "Evan, nyalakan output tiga. Evan, matikan output tiga. "
                    "Evan, nyalakan output empat. Evan, matikan output empat."),
            temperature=0,
            response_format="json")
        text = t.text
    except Exception as exc:
        print("STT ERROR:", repr(exc))
        raise HTTPException(status_code=502, detail="Speech-to-text gagal")
    result = parse_command(text)
    print("VOICE:", text, "=>", result)
    return JSONResponse(content=result)

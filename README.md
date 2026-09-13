# Evan Smart Home Voice — ESP32-S3 + INMP441 + Railway

Versi ini **mengganti Groq Whisper dengan Google Speech Recognition** sebagai STT.
Tidak membutuhkan `GROQ_API_KEY`.

> Catatan: Google Speech Recognition yang dipakai oleh library `SpeechRecognition`
> adalah layanan online dan cocok untuk prototipe. Untuk produksi jangka panjang,
> gunakan provider STT resmi yang menyediakan API key/SLA.

## 8 perintah suara

- Evan, nyalakan output satu
- Evan, matikan output satu
- Evan, nyalakan output dua
- Evan, matikan output dua
- Evan, nyalakan output tiga
- Evan, matikan output tiga
- Evan, nyalakan output empat
- Evan, matikan output empat

Parser juga menerima `relay` sebagai pengganti `output`, dan beberapa variasi
seperti `hidupkan`.

## Railway

Deploy folder `server/` sebagai service root.

Environment variable yang diperlukan:

- `DEVICE_TOKEN`

Tidak perlu `GROQ_API_KEY`.

Setelah Railway memberi domain, masukkan:

`https://DOMAIN-RAILWAY-KAMU/api/voice`

ke `SERVER_URL` di file:

`esp32/smart_home_voice.ino`

## Wiring INMP441

- VDD -> 3V3
- GND -> GND
- SCK -> GPIO15
- WS -> GPIO16
- SD -> GPIO17
- L/R -> GND

## Wiring relay 4 channel

- IN1 -> GPIO4
- IN2 -> GPIO5
- IN3 -> GPIO6
- IN4 -> GPIO7
- GND -> GND
- VCC -> supply 5V yang sesuai dengan modul relay

Mayoritas modul relay aktif LOW. Jika modul kamu aktif HIGH, ubah:

`RELAY_ON LOW` menjadi `RELAY_ON HIGH`

dan sebaliknya untuk `RELAY_OFF`.

## Arduino IDE

Install board ESP32 dari Espressif dan library:

- ArduinoJson

Library `WiFi`, `HTTPClient`, `WiFiClientSecure`, dan I2S berasal dari ESP32 core.

## Keamanan

Jangan uji awal dengan listrik PLN/mains. Gunakan beban DC tegangan rendah.
Jangan membuka kabel mains di breadboard.

# ESP32-S3 Smart Home Voice — Groq Whisper

Arsitektur:
INMP441 -> ESP32-S3 -> Wi-Fi -> Railway -> Groq Whisper -> Railway -> ESP32-S3 -> Relay 1-4

## Hardware

INMP441:
- VDD -> 3V3
- GND -> GND
- SCK -> GPIO15
- WS -> GPIO16
- SD -> GPIO17
- L/R -> GND

Relay 4CH:
- IN1 -> GPIO4
- IN2 -> GPIO5
- IN3 -> GPIO6
- IN4 -> GPIO7
- GND -> GND
- VCC -> 5V

Default relay logic in sketch: active LOW.

## Railway

Variables:
GROQ_API_KEY=gsk_...
DEVICE_TOKEN=buat-token-acak-sendiri

Deploy service dari folder `server`.

Generate public domain Railway, lalu set:
SERVER_URL = "https://DOMAIN-RAILWAY/api/voice"

## Groq

Model STT:
whisper-large-v3-turbo

API key hanya disimpan di Railway. Jangan taruh GROQ_API_KEY di ESP32.

## ESP32

Edit:
- WIFI_SSID
- WIFI_PASSWORD
- SERVER_URL
- DEVICE_TOKEN

Board yang umum:
ESP32S3 Dev Module

Perintah:
- Nyalakan output 1
- Matikan output 1
- Nyalakan output 2
- Matikan output 2
- Nyalakan output 3
- Matikan output 3
- Nyalakan output 4
- Matikan output 4

Juga mendukung variasi angka seperti:
- output satu
- relay dua
- output tiga
- relay empat

## Catatan keselamatan

Untuk pengujian awal gunakan beban DC tegangan rendah.
Jangan merakit 220V PLN di breadboard. Untuk listrik PLN gunakan enclosure, proteksi, terminal, kabel, relay dengan rating yang sesuai, dan prosedur keselamatan listrik yang benar.

## Catatan keamanan HTTPS

Sketch menggunakan `client.setInsecure()` untuk memudahkan prototipe HTTPS. Ini tidak memverifikasi sertifikat server. Untuk deployment permanen, gunakan verifikasi CA certificate.

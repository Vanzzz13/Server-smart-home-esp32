# Evan Smart Home Voice
ESP32-S3 + INMP441 + Relay 4CH + Railway + Groq Whisper.

8 commands:
- Evan, nyalakan output satu.
- Evan, matikan output satu.
- Evan, nyalakan output dua.
- Evan, matikan output dua.
- Evan, nyalakan output tiga.
- Evan, matikan output tiga.
- Evan, nyalakan output empat.
- Evan, matikan output empat.

Server menolak command tanpa wake word Evan.

Railway variables:
GROQ_API_KEY
DEVICE_TOKEN

Root Directory Railway: /server

INMP441:
VDD->3V3, GND->GND, SCK->GPIO15, WS->GPIO16, SD->GPIO17, L/R->GND

Relay:
IN1->GPIO4, IN2->GPIO5, IN3->GPIO6, IN4->GPIO7, GND->GND, VCC->5V supply.

Kode mengasumsikan relay active LOW.

Catatan: wake word Evan diproses setelah audio sampai di cloud STT; ini bukan wake-word detector lokal.

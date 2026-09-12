# Smart Home Voice - ESP32-S3 + INMP441 + Railway + Groq Whisper

Cloud-only voice control for 4 relays.

## Commands
- nyalakan output 1
- matikan output 1
- nyalakan output 2
- matikan output 2
- nyalakan output 3
- matikan output 3
- nyalakan output 4
- matikan output 4

## Railway
Deploy `server/` as the Railway service root directory.
Variables:
- GROQ_API_KEY
- DEVICE_TOKEN

Generate a public Railway domain, then put:
`https://YOUR-DOMAIN/api/voice`
into `esp32/smart_home_voice.ino`.

## Wiring
INMP441:
- VDD -> 3V3
- GND -> GND
- SCK -> GPIO15
- WS -> GPIO16
- SD -> GPIO17
- L/R -> GND

Relay:
- IN1 -> GPIO4
- IN2 -> GPIO5
- IN3 -> GPIO6
- IN4 -> GPIO7
- GND -> GND
- VCC -> suitable 5V supply

Most relay modules are active LOW; change RELAY_ON/OFF if yours is different.

## Safety
Do initial tests with a low-voltage DC load. Do not expose or breadboard mains wiring.

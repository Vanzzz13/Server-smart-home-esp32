#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "driver/i2s.h"

// ===== WIFI =====
const char* WIFI_SSID = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";

// ===== RAILWAY =====
const char* SERVER_URL =
    "https://YOUR-APP.up.railway.app/api/voice";
const char* DEVICE_TOKEN =
    "GANTI_DENGAN_DEVICE_TOKEN";

// ===== RELAY =====
#define RELAY1_PIN 4
#define RELAY2_PIN 5
#define RELAY3_PIN 6
#define RELAY4_PIN 7
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// ===== INMP441 =====
#define I2S_PORT I2S_NUM_0
#define I2S_BCLK 15
#define I2S_WS   16
#define I2S_DIN  17

#define SAMPLE_RATE 16000
#define RECORD_SECONDS 4
#define TOTAL_SAMPLES (SAMPLE_RATE * RECORD_SECONDS)
#define AUDIO_BYTES (TOTAL_SAMPLES * 2)

// Tune this value using Serial Monitor.
#define VOICE_THRESHOLD 1200
#define SPEECH_TRIGGER_FRAMES 3

int16_t* audioBuffer = nullptr;

void setRelay(int output, bool state) {
  int pin = -1;
  if (output == 1) pin = RELAY1_PIN;
  else if (output == 2) pin = RELAY2_PIN;
  else if (output == 3) pin = RELAY3_PIN;
  else if (output == 4) pin = RELAY4_PIN;
  else return;

  digitalWrite(pin, state ? RELAY_ON : RELAY_OFF);
  Serial.printf("OUTPUT %d = %s\n", output, state ? "ON" : "OFF");
}

void setupRelays() {
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  digitalWrite(RELAY3_PIN, RELAY_OFF);
  digitalWrite(RELAY4_PIN, RELAY_OFF);
}

void setupI2S() {
  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  config.sample_rate = SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  config.communication_format = I2S_COMM_FORMAT_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = 256;
  config.use_apll = false;
  config.tx_desc_auto_clear = false;
  config.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = I2S_BCLK;
  pins.ws_io_num = I2S_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = I2S_DIN;

  i2s_driver_install(I2S_PORT, &config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
}

float readAudioLevel() {
  int32_t samples[256];
  size_t bytesRead = 0;

  i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
  int count = bytesRead / sizeof(int32_t);
  if (count <= 0) return 0;

  double sum = 0;
  for (int i = 0; i < count; i++) {
    int32_t sample = samples[i] >> 14;
    sum += abs(sample);
  }
  return sum / count;
}

bool recordAudio() {
  if (!audioBuffer) return false;
  Serial.println("Recording...");

  size_t totalSamples = 0;
  while (totalSamples < TOTAL_SAMPLES) {
    int32_t temp[256];
    size_t bytesRead = 0;
    i2s_read(I2S_PORT, temp, sizeof(temp), &bytesRead, portMAX_DELAY);
    int count = bytesRead / sizeof(int32_t);

    for (int i = 0; i < count && totalSamples < TOTAL_SAMPLES; i++) {
      int32_t sample = temp[i] >> 14;
      if (sample > 32767) sample = 32767;
      if (sample < -32768) sample = -32768;
      audioBuffer[totalSamples++] = (int16_t)sample;
    }
  }
  Serial.println("Recording selesai.");
  return true;
}

void writeLE16(uint8_t* b, int o, uint16_t v) {
  b[o] = v & 0xFF; b[o+1] = (v >> 8) & 0xFF;
}

void writeLE32(uint8_t* b, int o, uint32_t v) {
  b[o] = v & 0xFF; b[o+1] = (v >> 8) & 0xFF;
  b[o+2] = (v >> 16) & 0xFF; b[o+3] = (v >> 24) & 0xFF;
}

void createWav(uint8_t* wav, size_t audioSize) {
  memset(wav, 0, 44);
  memcpy(wav, "RIFF", 4);
  writeLE32(wav, 4, 36 + audioSize);
  memcpy(wav + 8, "WAVE", 4);
  memcpy(wav + 12, "fmt ", 4);
  writeLE32(wav, 16, 16);
  writeLE16(wav, 20, 1);
  writeLE16(wav, 22, 1);
  writeLE32(wav, 24, SAMPLE_RATE);
  writeLE32(wav, 28, SAMPLE_RATE * 2);
  writeLE16(wav, 32, 2);
  writeLE16(wav, 34, 16);
  memcpy(wav + 36, "data", 4);
  writeLE32(wav, 40, audioSize);
}

bool sendAudio() {
  const size_t wavSize = 44 + AUDIO_BYTES;
  uint8_t* wav = (uint8_t*)malloc(wavSize);
  if (!wav) {
    Serial.println("Gagal allocate WAV");
    return false;
  }

  createWav(wav, AUDIO_BYTES);
  memcpy(wav + 44, audioBuffer, AUDIO_BYTES);

  WiFiClientSecure client;
  // Prototype only. For production, pin/verify Railway's CA certificate.
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, SERVER_URL)) {
    Serial.println("HTTP begin gagal");
    free(wav);
    return false;
  }

  http.setTimeout(30000);
  http.addHeader("Content-Type", "audio/wav");
  http.addHeader("Authorization", String("Bearer ") + DEVICE_TOKEN);

  int code = http.POST(wav, wavSize);
  Serial.printf("HTTP status: %d\n", code);

  if (code > 0) {
    String response = http.getString();
    Serial.println(response);

    if (code == 200) {
      JsonDocument doc;
      if (deserializeJson(doc, response) == DeserializationError::Ok) {
        bool success = doc["success"] | false;
        const char* command = doc["command"] | "";
        int output = doc["output"] | 0;
        const char* state = doc["state"] | "";

        if (success && strcmp(command, "relay") == 0 &&
            output >= 1 && output <= 4) {
          if (strcmp(state, "on") == 0) setRelay(output, true);
          else if (strcmp(state, "off") == 0) setRelay(output, false);
        }
      }
    }
  } else {
    Serial.println(http.errorToString(code));
  }

  http.end();
  free(wav);
  return code == 200;
}

void connectWiFi() {
  Serial.println("Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void waitForVoice() {
  int speechFrames = 0;

  while (true) {
    if (WiFi.status() != WL_CONNECTED) connectWiFi();

    float level = readAudioLevel();
    Serial.printf("Level: %.0f\n", level);

    if (level > VOICE_THRESHOLD) speechFrames++;
    else speechFrames = 0;

    if (speechFrames >= SPEECH_TRIGGER_FRAMES) {
      Serial.println("Suara terdeteksi!");
      break;
    }
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  setupRelays();
  setupI2S();
  connectWiFi();

  audioBuffer = (int16_t*)malloc(AUDIO_BYTES);
  if (!audioBuffer) {
    Serial.println("Gagal membuat audio buffer!");
    while (true) delay(1000);
  }

  Serial.printf("Audio buffer: %u bytes\n", (unsigned)AUDIO_BYTES);
  Serial.println("SYSTEM READY");
}

void loop() {
  waitForVoice();
  delay(100);
  if (recordAudio()) sendAudio();
  delay(1000);
}

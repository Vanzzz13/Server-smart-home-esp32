#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "driver/i2s.h"

// =====================
// WIFI
// =====================
const char* WIFI_SSID = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";

// =====================
// RAILWAY
// Ganti domain Railway lo.
// =====================
const char* SERVER_URL =
    "https://YOUR-RAILWAY-DOMAIN.up.railway.app/api/voice";

// HARUS SAMA dengan DEVICE_TOKEN di Railway.
const char* DEVICE_TOKEN =
    "GANTI_DEVICE_TOKEN";

// =====================
// RELAY
// =====================
#define RELAY_1 4
#define RELAY_2 5
#define RELAY_3 6
#define RELAY_4 7

#define RELAY_ON LOW
#define RELAY_OFF HIGH

// =====================
// INMP441 / I2S
// =====================
#define I2S_PORT I2S_NUM_0
#define I2S_BCLK 15
#define I2S_WS   16
#define I2S_DIN  17

// =====================
// AUDIO
// =====================
#define SAMPLE_RATE 16000
#define RECORD_SECONDS 4
#define BITS_PER_SAMPLE 16
#define CHANNELS 1

#define TOTAL_SAMPLES (SAMPLE_RATE * RECORD_SECONDS)
#define AUDIO_BYTES (TOTAL_SAMPLES * 2)

// =====================
// VOICE DETECTION
// =====================
#define VOICE_THRESHOLD 1200
#define VOICE_FRAMES_REQUIRED 3

uint8_t* audioBuffer = nullptr;

void setupI2S() {
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate = SAMPLE_RATE;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 8;
    cfg.dma_buf_len = 256;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = false;
    cfg.fixed_mclk = 0;

    i2s_pin_config_t pins = {};
    pins.bck_io_num = I2S_BCLK;
    pins.ws_io_num = I2S_WS;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = I2S_DIN;

    i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
    i2s_set_pin(I2S_PORT, &pins);
    i2s_zero_dma_buffer(I2S_PORT);
}

void setupRelays() {
    pinMode(RELAY_1, OUTPUT);
    pinMode(RELAY_2, OUTPUT);
    pinMode(RELAY_3, OUTPUT);
    pinMode(RELAY_4, OUTPUT);

    digitalWrite(RELAY_1, RELAY_OFF);
    digitalWrite(RELAY_2, RELAY_OFF);
    digitalWrite(RELAY_3, RELAY_OFF);
    digitalWrite(RELAY_4, RELAY_OFF);
}

void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

void writeLE16(uint8_t* b, uint32_t o, uint16_t v) {
    b[o] = v & 0xFF;
    b[o + 1] = (v >> 8) & 0xFF;
}

void writeLE32(uint8_t* b, uint32_t o, uint32_t v) {
    b[o] = v & 0xFF;
    b[o + 1] = (v >> 8) & 0xFF;
    b[o + 2] = (v >> 16) & 0xFF;
    b[o + 3] = (v >> 24) & 0xFF;
}

void createWavHeader(uint8_t* wav, uint32_t dataSize) {
    memcpy(wav, "RIFF", 4);
    writeLE32(wav, 4, 36 + dataSize);
    memcpy(wav + 8, "WAVE", 4);
    memcpy(wav + 12, "fmt ", 4);

    writeLE32(wav, 16, 16);       // PCM chunk size
    writeLE16(wav, 20, 1);        // PCM
    writeLE16(wav, 22, CHANNELS);
    writeLE32(wav, 24, SAMPLE_RATE);

    uint32_t byteRate =
        SAMPLE_RATE * CHANNELS * BITS_PER_SAMPLE / 8;
    writeLE32(wav, 28, byteRate);

    uint16_t blockAlign =
        CHANNELS * BITS_PER_SAMPLE / 8;
    writeLE16(wav, 32, blockAlign);
    writeLE16(wav, 34, BITS_PER_SAMPLE);

    memcpy(wav + 36, "data", 4);
    writeLE32(wav, 40, dataSize);
}

bool recordAudio() {
    if (!audioBuffer) {
        audioBuffer = (uint8_t*)malloc(AUDIO_BYTES + 44);
    }

    if (!audioBuffer) {
        Serial.println("Memory allocation failed!");
        return false;
    }

    createWavHeader(audioBuffer, AUDIO_BYTES);

    uint8_t* destination = audioBuffer + 44;
    size_t bytesWritten = 0;

    Serial.println("Recording...");

    while (bytesWritten < AUDIO_BYTES) {
        int32_t samples[256];
        size_t bytesRead = 0;

        esp_err_t result = i2s_read(
            I2S_PORT,
            samples,
            sizeof(samples),
            &bytesRead,
            portMAX_DELAY
        );

        if (result != ESP_OK || bytesRead == 0) {
            continue;
        }

        size_t sampleCount = bytesRead / sizeof(int32_t);

        for (size_t i = 0;
             i < sampleCount && bytesWritten < AUDIO_BYTES;
             i++) {

            int32_t value = samples[i] >> 11;

            if (value > 32767) value = 32767;
            if (value < -32768) value = -32768;

            int16_t pcm = (int16_t)value;

            destination[bytesWritten++] = pcm & 0xFF;

            if (bytesWritten < AUDIO_BYTES) {
                destination[bytesWritten++] =
                    (pcm >> 8) & 0xFF;
            }
        }
    }

    Serial.println("Recording finished.");
    return true;
}

bool detectVoice() {
    int32_t samples[256];
    size_t bytesRead = 0;

    esp_err_t result = i2s_read(
        I2S_PORT,
        samples,
        sizeof(samples),
        &bytesRead,
        100 / portTICK_PERIOD_MS
    );

    if (result != ESP_OK || bytesRead == 0) {
        return false;
    }

    size_t count = bytesRead / sizeof(int32_t);
    if (count == 0) return false;

    long long total = 0;

    for (size_t i = 0; i < count; i++) {
        int32_t value = samples[i] >> 11;
        if (value < 0) value = -value;
        total += value;
    }

    int average = total / count;
    return average > VOICE_THRESHOLD;
}

void applyRelayCommand(int output, const String& state) {
    int pin = -1;

    if (output == 1) pin = RELAY_1;
    if (output == 2) pin = RELAY_2;
    if (output == 3) pin = RELAY_3;
    if (output == 4) pin = RELAY_4;

    if (pin < 0) return;

    if (state == "on") {
        digitalWrite(pin, RELAY_ON);
        Serial.printf("Relay %d ON\n", output);
    } else if (state == "off") {
        digitalWrite(pin, RELAY_OFF);
        Serial.printf("Relay %d OFF\n", output);
    }
}

bool sendAudio() {
    if (!audioBuffer) return false;

    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    WiFiClientSecure client;

    // Prototype only. This disables TLS certificate verification.
    // For permanent deployment, use CA certificate verification.
    client.setInsecure();

    HTTPClient http;

    if (!http.begin(client, SERVER_URL)) {
        Serial.println("HTTP begin failed!");
        return false;
    }

    http.addHeader(
        "Authorization",
        String("Bearer ") + DEVICE_TOKEN
    );

    http.addHeader("Content-Type", "audio/wav");

    Serial.println("Uploading audio...");

    int code = http.POST(
        audioBuffer,
        AUDIO_BYTES + 44
    );

    Serial.printf("HTTP code: %d\n", code);

    if (code > 0) {
        String response = http.getString();

        Serial.println("Server response:");
        Serial.println(response);

        int outputPos = response.indexOf("\"output\":");
        int statePos = response.indexOf("\"state\":\"");

        if (outputPos >= 0 && statePos >= 0) {
            int output = response[outputPos + 9] - '0';
            int stateStart = statePos + 9;

            String state = response.substring(
                stateStart,
                stateStart + 2
            );

            if (output >= 1 && output <= 4) {
                applyRelayCommand(output, state);
            }
        }

        http.end();
        return true;
    }

    http.end();
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println(" ESP32 SMART HOME VOICE");
    Serial.println(" Groq Whisper Edition");
    Serial.println("================================");

    setupRelays();
    setupI2S();
    connectWiFi();
}

void loop() {
    static int voiceFrames = 0;

    if (detectVoice()) {
        voiceFrames++;

        if (voiceFrames >= VOICE_FRAMES_REQUIRED) {
            voiceFrames = 0;

            if (recordAudio()) {
                sendAudio();
            }

            delay(1000);
        }
    } else {
        voiceFrames = 0;
    }

    delay(20);
}

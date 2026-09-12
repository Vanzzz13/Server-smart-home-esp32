#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "driver/i2s.h"

const char* WIFI_SSID = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";
const char* SERVER_URL = "https://YOUR-APP.up.railway.app/api/voice";
const char* DEVICE_TOKEN = "GANTI_DENGAN_DEVICE_TOKEN";

#define RELAY1_PIN 4
#define RELAY2_PIN 5
#define RELAY3_PIN 6
#define RELAY4_PIN 7
#define RELAY_ON LOW
#define RELAY_OFF HIGH

#define I2S_PORT I2S_NUM_0
#define I2S_BCLK 15
#define I2S_WS 16
#define I2S_DIN 17

#define SAMPLE_RATE 16000
#define RECORD_SECONDS 4
#define TOTAL_SAMPLES (SAMPLE_RATE * RECORD_SECONDS)
#define AUDIO_BYTES (TOTAL_SAMPLES * 2)
#define VOICE_THRESHOLD 1200
#define SPEECH_TRIGGER_FRAMES 3

int16_t* audioBuffer = nullptr;

void setRelay(int output, bool state) {
  int pin = output==1?RELAY1_PIN:output==2?RELAY2_PIN:output==3?RELAY3_PIN:output==4?RELAY4_PIN:-1;
  if(pin<0) return;
  digitalWrite(pin, state ? RELAY_ON : RELAY_OFF);
  Serial.printf("OUTPUT %d = %s\n", output, state?"ON":"OFF");
}

void setupRelays() {
  pinMode(RELAY1_PIN,OUTPUT); pinMode(RELAY2_PIN,OUTPUT);
  pinMode(RELAY3_PIN,OUTPUT); pinMode(RELAY4_PIN,OUTPUT);
  digitalWrite(RELAY1_PIN,RELAY_OFF); digitalWrite(RELAY2_PIN,RELAY_OFF);
  digitalWrite(RELAY3_PIN,RELAY_OFF); digitalWrite(RELAY4_PIN,RELAY_OFF);
}

void setupI2S() {
  i2s_config_t c = {};
  c.mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_RX);
  c.sample_rate=SAMPLE_RATE; c.bits_per_sample=I2S_BITS_PER_SAMPLE_32BIT;
  c.channel_format=I2S_CHANNEL_FMT_ONLY_LEFT;
  c.communication_format=I2S_COMM_FORMAT_I2S;
  c.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1; c.dma_buf_count=8; c.dma_buf_len=256;
  c.use_apll=false; c.tx_desc_auto_clear=false; c.fixed_mclk=0;
  i2s_pin_config_t p={}; p.bck_io_num=I2S_BCLK; p.ws_io_num=I2S_WS;
  p.data_out_num=I2S_PIN_NO_CHANGE; p.data_in_num=I2S_DIN;
  i2s_driver_install(I2S_PORT,&c,0,NULL); i2s_set_pin(I2S_PORT,&p);
  i2s_zero_dma_buffer(I2S_PORT);
}

float readAudioLevel() {
  int32_t s[256]; size_t n=0;
  i2s_read(I2S_PORT,s,sizeof(s),&n,portMAX_DELAY);
  int count=n/sizeof(int32_t); if(count<=0) return 0;
  double sum=0;
  for(int i=0;i<count;i++) sum += abs(s[i]>>14);
  return sum/count;
}

bool recordAudio() {
  if(!audioBuffer) return false;
  Serial.println("Recording...");
  size_t total=0;
  while(total<TOTAL_SAMPLES) {
    int32_t t[256]; size_t n=0;
    i2s_read(I2S_PORT,t,sizeof(t),&n,portMAX_DELAY);
    int count=n/sizeof(int32_t);
    for(int i=0;i<count && total<TOTAL_SAMPLES;i++) {
      int32_t x=t[i]>>14; if(x>32767)x=32767; if(x<-32768)x=-32768;
      audioBuffer[total++]=(int16_t)x;
    }
  }
  Serial.println("Recording selesai.");
  return true;
}

void le16(uint8_t*b,int o,uint16_t v){b[o]=v&255;b[o+1]=(v>>8)&255;}
void le32(uint8_t*b,int o,uint32_t v){b[o]=v&255;b[o+1]=(v>>8)&255;b[o+2]=(v>>16)&255;b[o+3]=(v>>24)&255;}

void createWav(uint8_t*w,size_t size){
  memset(w,0,44); memcpy(w,"RIFF",4); le32(w,4,36+size); memcpy(w+8,"WAVE",4);
  memcpy(w+12,"fmt ",4); le32(w,16,16); le16(w,20,1); le16(w,22,1);
  le32(w,24,SAMPLE_RATE); le32(w,28,SAMPLE_RATE*2); le16(w,32,2); le16(w,34,16);
  memcpy(w+36,"data",4); le32(w,40,size);
}

bool sendAudio() {
  size_t wavSize=44+AUDIO_BYTES; uint8_t* wav=(uint8_t*)malloc(wavSize);
  if(!wav){Serial.println("Gagal allocate WAV");return false;}
  createWav(wav,AUDIO_BYTES); memcpy(wav+44,audioBuffer,AUDIO_BYTES);
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  if(!http.begin(client,SERVER_URL)){free(wav);return false;}
  http.setTimeout(30000); http.addHeader("Content-Type","audio/wav");
  http.addHeader("Authorization",String("Bearer ")+DEVICE_TOKEN);
  int code=http.POST(wav,wavSize); Serial.printf("HTTP status: %d\n",code);
  if(code>0){
    String r=http.getString(); Serial.println(r);
    if(code==200){
      JsonDocument doc;
      if(deserializeJson(doc,r)==DeserializationError::Ok){
        bool ok=doc["success"]|false; int out=doc["output"]|0;
        const char* cmd=doc["command"]|""; const char* st=doc["state"]|"";
        if(ok && strcmp(cmd,"relay")==0 && out>=1 && out<=4){
          if(strcmp(st,"on")==0)setRelay(out,true);
          else if(strcmp(st,"off")==0)setRelay(out,false);
        }
      }
    }
  } else Serial.println(http.errorToString(code));
  http.end(); free(wav); return code==200;
}

void connectWiFi(){
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  Serial.println("Connecting WiFi...");
  while(WiFi.status()!=WL_CONNECTED){delay(500);Serial.print(".");}
  Serial.println(); Serial.print("IP: "); Serial.println(WiFi.localIP());
}

void waitForVoice(){
  int frames=0;
  while(true){
    if(WiFi.status()!=WL_CONNECTED)connectWiFi();
    float level=readAudioLevel(); Serial.printf("Level: %.0f\n",level);
    if(level>VOICE_THRESHOLD)frames++; else frames=0;
    if(frames>=SPEECH_TRIGGER_FRAMES){Serial.println("Suara terdeteksi!");break;}
    delay(10);
  }
}

void setup(){
  Serial.begin(115200); delay(1000); setupRelays(); setupI2S(); connectWiFi();
  audioBuffer=(int16_t*)malloc(AUDIO_BYTES);
  if(!audioBuffer){Serial.println("Gagal membuat audio buffer!");while(true)delay(1000);}
  Serial.println("SYSTEM READY - wake word: Evan");
}

void loop(){
  waitForVoice(); delay(100);
  if(recordAudio())sendAudio();
  delay(1000);
}

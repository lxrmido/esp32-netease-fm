#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"
#include "AudioOutputI2S.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "config.h"

const char* url_prefix = URL_PREFIX;
const char* wifi_ssid = WIFI_SSID;
const char* wifi_password = WIFI_PASSWORD;
const char* login_phone = LOGIN_PHONE;
const char* login_password =  LOGIN_PASSWORD;

const int nextButton = 13;

char token[256];
int musicId;
char musicUrl[1024];

AudioGeneratorMP3 *mp3;
AudioFileSourceHTTPStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

bool buttonClicked = 0;

void IRAM_ATTR nextButtonInterrupt() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200) 
  {
    Serial.println("Next Clicked");
    buttonClicked = 1;
  }
  last_interrupt_time = interrupt_time;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(nextButton, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(nextButton), nextButtonInterrupt, FALLING);
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
 
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
 
  // Print the IP address
  Serial.print("IP: ");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  login() && getMusic() && getMusicUrl() && playMusic();
}

bool login(){
  while (true){
    HTTPClient http;
    char url[128];
    sprintf(url, "%s/login/cellphone?phone=%s&password=%s", url_prefix, login_phone, login_password);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.GET();
    String payload = http.getString();
    Serial.print("HTTP Status:");
    Serial.println(httpCode);
    Serial.print("HTTP payload:");
    Serial.println(payload);
    http.end();
    if(httpCode == 200){
      Serial.println("logined success");
      const size_t bufferSize = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 370;
      DynamicJsonBuffer jsonBuffer(bufferSize);
      JsonObject& root = jsonBuffer.parseObject(payload);
      if (!root.success()) {
        Serial.println("JSON parsing failed!");
        delay(1000);
      }
      strcpy(token, root["token"]);
      Serial.print("Token:");
      Serial.println(token);
      return true;
    }else{
      Serial.println("login failed");
      delay(1000);
    }
  }
}

bool getMusic(){
  while (true){
    HTTPClient http;
    char url[128];
    sprintf(url, "%s/personal_fm?t=%d", url_prefix, random(1, 100000));
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.GET();
    String payload = http.getString();
    Serial.print("HTTP Status:");
    Serial.println(httpCode);
    Serial.print("HTTP payload:");
    Serial.println(payload);
    http.end();
    if(httpCode == 200){
      Serial.println("Get music success");
      const size_t bufferSize = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 370;
      DynamicJsonBuffer jsonBuffer(bufferSize);
      JsonObject& root = jsonBuffer.parseObject(payload);
      if (!root.success()) {
        Serial.println("JSON parsing failed!");
        delay(1000);
      }
      musicId = root["data"][0]["id"];
      Serial.print("id:");
      Serial.println(musicId);
      char musicIdStr[32];
      sprintf(musicIdStr, "ID:%d", musicId);
      return true;
    }else{
      Serial.println("personal_fm failed");
      delay(1000);
    }
  }
}


bool getMusicUrl(){
  while (true){
    HTTPClient http;
    char url[1024];
    sprintf(url, "%s/song/url?br=320000&id=%d", url_prefix, musicId);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.GET();
    String payload = http.getString();
    Serial.print("HTTP Status:");
    Serial.println(httpCode);
    Serial.print("HTTP payload:");
    Serial.println(payload);
    http.end();
    if(httpCode == 200){
      Serial.println("Get url success");
      const size_t bufferSize = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 370;
      DynamicJsonBuffer jsonBuffer(bufferSize);
      JsonObject& root = jsonBuffer.parseObject(payload);
      if (!root.success()) {
        Serial.println("JSON parsing failed!");
        delay(1000);
      }
      strcpy(musicUrl, root["data"][0]["url"]);
      Serial.print("url:");
      Serial.println(musicUrl);
      return true;
    }else{
      Serial.println("song/url failed");
      delay(1000);
    }
  }
}

bool playMusic(){
  file = new AudioFileSourceHTTPStream(musicUrl);
  buff = new AudioFileSourceBuffer(file, 384*1024);
  out = new AudioOutputI2S(0, 1);
  mp3 = new AudioGeneratorMP3();
  mp3->begin(buff, out);
}

void stopPlaying(){
  if (buff) {
    buff->close();
    delete buff;
    buff = NULL;
  }
  if (file) {
    file->close();
    delete file;
    file = NULL;
  }
  if (mp3 && mp3->isRunning()) {
      mp3->stop();
  }
}
 
void loop() {
  if (buttonClicked > 0) {
    Serial.println("Next needed.");
    stopPlaying();
    Serial.println("Next.");
    getMusic() && getMusicUrl() && playMusic();
    buttonClicked = 0;
  } else {
    if (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        getMusic() && getMusicUrl() && playMusic();
      } 
    } else {
      Serial.printf("MP3 done\n");
      delay(1000);
    }
  }
}

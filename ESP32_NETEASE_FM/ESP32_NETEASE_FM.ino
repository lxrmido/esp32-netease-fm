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
const int pauseButton = 35;
const int likeButton = 34;

bool playing = false;

char token[256];
int musicId;
char musicUrl[1024];
char requestUrl[256];

char cookie[1024];

const char * headerkeys[] = {"set-cookie"};
size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);

AudioGeneratorMP3 *mp3;
AudioFileSourceHTTPStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

int buttonClicked = 0;

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
void IRAM_ATTR pauseButtonInterrupt() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200) 
  {
    Serial.println("Pause Clicked");
    buttonClicked = 2;
  }
  last_interrupt_time = interrupt_time;
}
void IRAM_ATTR likeButtonInterrupt() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200) 
  {
    Serial.println("Like Clicked");
    buttonClicked = 3;
  }
  last_interrupt_time = interrupt_time;
}

const int preallocateBufferSize = 16*1024;
const int preallocateCodecSize = 85332;
void *preallocateBuffer = NULL;
void *preallocateCodec = NULL;

void setup() {
  Serial.begin(115200);
  delay(500);
  preallocateBuffer = malloc(preallocateBufferSize);
  preallocateCodec = malloc(preallocateCodecSize);
  out = new AudioOutputI2S(0, 1);
  pinMode(nextButton, INPUT_PULLUP);
  pinMode(pauseButton, INPUT_PULLUP);
  pinMode(likeButton, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(nextButton), nextButtonInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(pauseButton), pauseButtonInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(likeButton), likeButtonInterrupt, FALLING);
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

  login() && getMusicAndUrl() && playMusic();
}

void like(){
  HTTPClient http;
  out->SetGain(0.01);
  sprintf(requestUrl, "%s/like?id=%d&t=%d", url_prefix, musicId, random(1, 100000));
  http.collectHeaders(headerkeys, headerkeyssize);
  http.begin(requestUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Cookie", cookie);
  int httpCode = http.GET();
  String payload = http.getString();
  Serial.print("HTTP Status:");
  Serial.println(httpCode);
  Serial.print("HTTP payload:");
  Serial.println(payload);
  http.end();
  if(httpCode == 200){
    Serial.println("liked");
  }else{
    Serial.println("like failed");
  }
  out->SetGain(1.0);
}

bool login(){
  while (true){
    HTTPClient http;
    sprintf(requestUrl, "%s/login/cellphone?phone=%s&password=%s", url_prefix, login_phone, login_password);
    http.collectHeaders(headerkeys, headerkeyssize);
    http.begin(requestUrl);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.GET();
    String payload = http.getString();
    Serial.print("HTTP Status:");
    Serial.println(httpCode);
    Serial.print("HTTP payload:");
    Serial.println(payload);
    http.end();
    if(httpCode == 200){
      strcpy(cookie, http.header("set-cookie").c_str());
      Serial.println(cookie);
      Serial.println("logined success");
      const size_t bufferSize = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 370;
      DynamicJsonBuffer jsonBuffer(bufferSize);
      JsonObject& root = jsonBuffer.parseObject(payload);
      if (!root.success()) {
        Serial.println("JSON parsing failed!");
        delay(1000);
        continue;
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

bool getMusicAndUrl()
{
  while (true) {
    if (getMusic() && getMusicUrl()) {
      return true;
    }
    delay(5000);
  }
  return false;
}

bool getMusic()
{
  HTTPClient http;
  sprintf(requestUrl, "%s/personal_fm?t=%d", url_prefix, random(1, 100000));
  http.collectHeaders(headerkeys, headerkeyssize);
  http.begin(requestUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Cookie", cookie);
  int httpCode = http.GET();
  String payload = http.getString();
  Serial.print("HTTP Status:");
  Serial.println(httpCode);
  Serial.print("HTTP payload:");
  Serial.println(payload);
  http.end();
  if (httpCode == 200)
  {
    Serial.println("Get music success");
    const size_t bufferSize = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 370;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject &root = jsonBuffer.parseObject(payload);
    if (!root.success())
    {
      Serial.println("JSON parsing failed!");
    }
    else
    {
      musicId = root["data"][0]["id"];
      Serial.print("id:");
      Serial.println(musicId);
      char musicIdStr[32];
      sprintf(musicIdStr, "ID:%d", musicId);
      return true;
    }
  }
  else
  {
    Serial.println("personal_fm failed");
    return false;
  }
}

bool getMusicUrl()
{
  HTTPClient http;
  sprintf(requestUrl, "%s/song/url?br=320000&id=%d", url_prefix, musicId);
  http.collectHeaders(headerkeys, headerkeyssize);
  http.begin(requestUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Cookie", cookie);
  int httpCode = http.GET();
  String payload = http.getString();
  Serial.print("HTTP Status:");
  Serial.println(httpCode);
  Serial.print("HTTP payload:");
  Serial.println(payload);
  http.end();
  if (httpCode == 200)
  {
    Serial.println("Get url success");
    const size_t bufferSize = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 370;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject &root = jsonBuffer.parseObject(payload);
    if (!root.success())
    {
      Serial.println("JSON parsing failed!");
    }
    else
    {
      strcpy(musicUrl, root["data"][0]["url"]);
      Serial.print("url:");
      Serial.println(musicUrl);
      return true;
    }
  }
  else
  {
    Serial.println("song/url failed");
    return false;
  }
}

bool playMusic(){
  Serial.println("Start new");
  file = new AudioFileSourceHTTPStream(musicUrl);
  buff = new AudioFileSourceBuffer(file, preallocateBuffer, preallocateBufferSize);
  
  mp3 = new AudioGeneratorMP3(preallocateCodec, preallocateCodecSize);
  mp3->begin(buff, out);
  playing = true;
  out->SetGain(1.0);
}

bool next() {
  out->SetGain(0.01);
  out->stop();
  getMusicAndUrl() && playMusic();
}

void loop() {
  static int lastms = 0;
  if (millis()-lastms > 1000) {
    lastms = millis();
    Serial.printf("Running for %d ms...\n", lastms);
    Serial.flush();
  }
  if (buttonClicked == 1) {
    Serial.println("Next needed.");
    next();
    buttonClicked = 0;
  } else if (buttonClicked == 2) {
    if (playing) {
      Serial.println("Pause");
      out->stop(); 
    } else {
      Serial.println("Play");
      out->begin();
    }
    playing = !playing;
    buttonClicked = 0;
  }else if (buttonClicked == 3) {
    like();
    buttonClicked = 0;
  }else {
    if (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        next();
      } 
    } else {
      Serial.printf("MP3 done\n");
      delay(1000);
    }
  }
}

# 用ESP32做一个网易云音乐私人FM播放器

## 功能说明

在程序中烧录WiFi的SSID、密码及网易云音乐的账号密码，使用ESP32的内置DAC，外接功放及三个按键（可选，分别实现收藏、下一首、暂停），通电自动登录并播放私人FM

## 前置条件

1. 由于我未能解决在ESP32上直接完成登录，所以首先需要在一台局域网PC、树莓派或云服务器上部署一个基于 (NeteaseCloudMusicApi)[https://github.com/Binaryify/NeteaseCloudMusicApi] 的API服务
2. 需要一块ESP32开发板

## 安装支持库

1. 安装 `ArduinoJson`（可以在Arduino库中直接搜索安装）
2. 安装 [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio)

## 接线说明

|端口|接线|
|---|---|
|GPIO25|功放左声道+|
|GPIO26|功放右声道+|
|GND|功放GND、按键GND|
|3V|按键VCC|
|GPIO33|暂停按键|
|GPIO34|收藏按键|
|GPIO35|下一首按键|

在GPIO25、GPIO26与功放之间添加一个1kΩ的电阻可有效降低底噪、提升高频清晰度，不加也不会烧板子

推荐使用基于PAM8403、PAM8406的功放板（很便宜），通常这两类功放板使用USB或5V电源供电、可以与ESP32开发板共用电源，也可以外接DAC（大幅提升音质）或者不使用功放电路直接驱动喇叭（效果一言难尽），具体方法查看[ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio)的说明

## 使用方法

1. 部署好(NeteaseCloudMusicApi)[https://github.com/Binaryify/NeteaseCloudMusicApi]
2. 复制一个 `config.h.example` 为 `config.h`，在里边填入你的各种账号密码和`NeteaseCloudMusicApi`服务的地址
3. 烧录程序、接线、通电运行

## 常见问题

### 1. 点击收藏按钮时提示需要登录

这是你使用的ESP32库中的HTTPClient不兼容网易云音乐的cookies格式引起的，编辑器打开ESP32库文件夹中的`HTTPClient.cpp`，找到：

```cpp
    for(size_t i = 0; i < _headerKeysCount; i++) {
        if(_currentHeaders[i].key.equalsIgnoreCase(headerName)) {
            _currentHeaders[i].value = headerValue;
            break;
        }
    }
```
把它修改为：

```cpp
    for(size_t i = 0; i < _headerKeysCount; i++) {
        if (_currentHeaders[i].key.equalsIgnoreCase(headerName)) {
            if (!_currentHeaders[i].value.isEmpty()) {
                // Existing value, append this one with a comma
                _currentHeaders[i].value += ',';
                _currentHeaders[i].value += headerValue;
            } else {
                _currentHeaders[i].value = headerValue;
            }
            break; // We found a match, stop looking
        }
    }
```

如果依然失败，把上面代码中的`','`修改为`';'`。

### 2. 播放完成后不会跳转下一首

这是一个在部分macos下编译程序时会出现的问题，原因大概是ESP8266Audio中某些逻辑未能真正检测出MP3播放已经完成，解决方法是打开ESP8266Audio中的`AudioGeneratorMP3.cpp`，找到：

```cpp
bool AudioGeneratorMP3::loop()
{
  if (!running) goto done; // Nothing to do here!

  // First, try and push in the stored sample.  If we can't, then punt and try later
  if (!output->ConsumeSample(lastSample)) goto done; // Can't send, but no error detected

  // Try and stuff the buffer one sample at a time
  do
  {
    // Decode next frame if we're beyond the existing generated data
    if ( (samplePtr >= synth->pcm.length) && (nsCount >= nsCountMax) ) {
retry:
      if (Input() == MAD_FLOW_STOP) {
        return false;
      }

      if (!DecodeNextFrame()) {
        goto retry;
      }
      samplePtr = 9999;
      nsCount = 0;
    }

    if (!GetOneSample(lastSample)) {
      audioLogger->printf_P(PSTR("G1S failed\n"));
      running = false;
      goto done;
    }
  } while (running && output->ConsumeSample(lastSample));

done:
  file->loop();
  output->loop();

  return running;
}
```

修改为：

```cpp

bool AudioGeneratorMP3::loop()
{
  static int failedCount = 0;
  if (!running) goto done; // Nothing to do here!

  // First, try and push in the stored sample.  If we can't, then punt and try later
  if (!output->ConsumeSample(lastSample)) goto done; // Can't send, but no error detected

  // Try and stuff the buffer one sample at a time
  do
  {
    // Decode next frame if we're beyond the existing generated data
    if ( (samplePtr >= synth->pcm.length) && (nsCount >= nsCountMax) ) {
retry:
      if (++failedCount > 100) {
        failedCount = 0;
        return false;
      }
      if (Input() == MAD_FLOW_STOP) {
        return false;
      }

      if (!DecodeNextFrame()) {
        goto retry;
      }
      samplePtr = 9999;
      nsCount = 0;
    }

    if (!GetOneSample(lastSample)) {
      audioLogger->printf_P(PSTR("G1S failed\n"));
      running = false;
      goto done;
    }
  } while (running && output->ConsumeSample(lastSample));

done:
  file->loop();
  output->loop();
  failedCount = 0;
  return running;
}
```

给它增加一个重试次数限制，避免进入死循环，可以解决问题。但是其底层原因我还没去研究。

### 3. 使用PlatformIO编译失败

通常是头文件未自动包含，看提示缺什么头文件，把缺失的引用进去

### 4. 各种杂音

切换音乐时的杂音原因未明，如果你知道请告诉我；点击收藏时的杂音是因为目前还没把收藏操作改为异步的、会暂停播放直到收藏完毕；底噪基本来自于电源。

如果想要提升音质，首先建议使用外部DAC（如PCM5102A、ES9018）；然后是更改代码，把请求的码率改为320000；最后是改善供电环境。


### 5. 其他

出现任何问题时，建议连接电脑，打开串口监视器看输出内容。
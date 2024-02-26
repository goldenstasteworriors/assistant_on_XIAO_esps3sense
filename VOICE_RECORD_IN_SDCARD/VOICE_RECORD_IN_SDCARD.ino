/* 
 * WAV Recorder for Seeed XIAO ESP32S3 Sense 
*/

#include <I2S.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Arduino.h>
#include "base64.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "cJSON.h"

// make changes as needed
#define RECORD_TIME   20  // seconds, The maximum value is 240
#define WAV_FILE_NAME "arduino_rec"

// do not change for best
#define SAMPLE_RATE 16000U
#define SAMPLE_BITS 16
#define WAV_HEADER_SIZE 44
#define VOLUME_GAIN 2

void setup() {
  Serial.begin(115200);
  while (!Serial) ;
  I2S.setAllPins(-1, 42, 41, -1, -1);
  if (!I2S.begin(PDM_MONO_MODE, SAMPLE_RATE, SAMPLE_BITS)) {
    Serial.println("Failed to initialize I2S!");
    while (1) ;
  }
  if(!SD.begin(21)){
    Serial.println("Failed to mount SD Card!");
    while (1) ;
  }
  record_wav();
}

void loop() {
  delay(1000);
  Serial.printf(".");
}

void record_wav()
{
  uint32_t sample_size = 0;
  uint32_t record_size = (SAMPLE_RATE * SAMPLE_BITS / 8) * RECORD_TIME;
  uint8_t *rec_buffer = NULL;
  Serial.printf("Ready to start recording ...\n");

  File file = SD.open("/"WAV_FILE_NAME".wav", FILE_WRITE);
  // Write the header to the WAV file
  uint8_t wav_header[WAV_HEADER_SIZE];
  generate_wav_header(wav_header, record_size, SAMPLE_RATE);
  file.write(wav_header, WAV_HEADER_SIZE);

  // PSRAM malloc for recording
  rec_buffer = (uint8_t *)ps_malloc(record_size);
  if (rec_buffer == NULL) {
    Serial.printf("malloc failed!\n");
    while(1) ;
  }
  Serial.printf("Buffer: %d bytes\n", ESP.getPsramSize() - ESP.getFreePsram());

  // Start recording
  esp_i2s::i2s_read(esp_i2s::I2S_NUM_0, rec_buffer, record_size, &sample_size, portMAX_DELAY);
  if (sample_size == 0) {
    Serial.printf("Record Failed!\n");
  } else {
    Serial.printf("Record %d bytes\n", sample_size);
  }

  // Increase volume
  for (uint32_t i = 0; i < sample_size; i += SAMPLE_BITS/8) {
    (*(uint16_t *)(rec_buffer+i)) <<= VOLUME_GAIN;
  }

  // Write data to the WAV file
  Serial.printf("Writing to the file ...\n");
  if (file.write(rec_buffer, record_size) != record_size)
    Serial.printf("Write file Failed!\n");

  free(rec_buffer);
  file.close();
  Serial.printf("The recording is over.\n");
  /*-------------------------------------------------------------------------------------------------------------------------------------*/

delay(1000);
  // 打开录音文件
    File wavFile = SD.open("/"WAV_FILE_NAME".wav", FILE_READ);
    if (!wavFile) {
        Serial.println("Failed to open WAV file!");
        return;
    }

    // 获取录音文件的大小
    size_t fileSize = wavFile.size();
    Serial.println("");
    Serial.printf("fileSize: %d\n",fileSize);
    Serial.println("");
    // 分配内存缓冲区以存储整个录音文件的内容
    uint8_t *wavData = (uint8_t *)malloc(fileSize);
    if (!wavData) {
        Serial.println("Failed to allocate memory for WAV data!");
        wavFile.close();
        return;
    }
        Serial.println("Succeed to allocate memory for WAV data!");
    // 将录音文件的内容读取到内存缓冲区中
    size_t bytesRead = wavFile.read(wavData, fileSize);
    if (bytesRead != fileSize) {
        Serial.println("Error reading WAV file!");
        free(wavData);
        wavFile.close();
        return;
    }
    wavFile.close();

    // 将WAV文件的内容转换为Base64编码
    String base64Data = base64::encode(wavData, fileSize);
    free(wavData); // 释放内存，避免内存泄漏

    // 构建JSON格式的数据
    char data_json[512]; // 修改为适当大小
    sprintf(data_json, "{\"format\":\"wav\",\"rate\":16000,\"channel\":1,\"token\":\"25.16c2e53845ebcba72f540f1847ba0bb9.315360000.2022587707.282335-49772519\",\"cuid\":\"123456\",\"len\":%d,\"speech\":\"%s\"}", fileSize, base64Data.c_str());

    // 发送HTTP POST请求到百度语音识别API并获取识别结果
    HTTPClient http_client;
    http_client.begin("http://vop.baidu.com/server_api");
    http_client.addHeader("Content-Type", "application/json");
    int httpCode = http_client.POST(data_json);

    // 处理HTTP响应
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http_client.getString();
            Serial.println(payload);
        }
    }
    else {
        Serial.printf("[HTTP] POST... failed, error: %s\n", http_client.errorToString(httpCode).c_str());
    }
    http_client.end();
}

void generate_wav_header(uint8_t *wav_header, uint32_t wav_size, uint32_t sample_rate)
{
  // See this for reference: http://soundfile.sapp.org/doc/WaveFormat/
  uint32_t file_size = wav_size + WAV_HEADER_SIZE - 8;
  uint32_t byte_rate = SAMPLE_RATE * SAMPLE_BITS / 8;
  const uint8_t set_wav_header[] = {
    'R', 'I', 'F', 'F', // ChunkID
    (uint8_t)file_size, (uint8_t)(file_size >> 8), (uint8_t)(file_size >> 16), (uint8_t)(file_size >> 24), // ChunkSize
    'W', 'A', 'V', 'E', // Format
    'f', 'm', 't', ' ', // Subchunk1ID
    0x10, 0x00, 0x00, 0x00, // Subchunk1Size (16 for PCM)
    0x01, 0x00, // AudioFormat (1 for PCM)
    0x01, 0x00, // NumChannels (1 channel)
    (uint8_t)sample_rate, (uint8_t)(sample_rate >> 8), (uint8_t)(sample_rate >> 16), (uint8_t)(sample_rate >> 24), // SampleRate
    (uint8_t)byte_rate, (uint8_t)(byte_rate >> 8), (uint8_t)(byte_rate >> 16),(uint8_t)(byte_rate >> 24), // ByteRate
    0x02, 0x00, // BlockAlign
    0x10, 0x00, // BitsPerSample (16 bits)
    'd', 'a', 't', 'a', // Subchunk2ID
    (uint8_t)wav_size, (uint8_t)(wav_size >> 8), (uint8_t)(wav_size >> 16), (uint8_t)(wav_size >> 24), // Subchunk2Size
  };
  memcpy(wav_header, set_wav_header, sizeof(set_wav_header));
}

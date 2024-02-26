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
#define RECORD_TIME   5  // seconds, The maximum value is 240
#define WAV_FILE_NAME "arduino_rec"

const char* ssid = "ykj";
const char* password = "604604604";
// do not change for best
#define SAMPLE_RATE 16000U
#define SAMPLE_BITS 16
#define WAV_HEADER_SIZE 44
#define VOLUME_GAIN 2

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println();
  Serial.println(WiFi.localIP());
}

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
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  initWiFi();
  record_wav_and_recognize();
}

void loop() {
  delay(1000);
  Serial.printf(".");
}

void record_wav_and_recognize()
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

  
  file.close();
  Serial.printf("The recording is over.\n");
  /*-------------------------------------------------------------------------------------------------------------------------------------*/


    // 构建JSON格式的数据
    char *data_json; // 修改为适当大小
    data_json = (char *)ps_malloc(record_size*3/2);
    
    memset(data_json,'\0',strlen(data_json));   //将数组清空
    strcat(data_json,"{");
    strcat(data_json,"\"format\":\"pcm\",");
    strcat(data_json,"\"rate\":16000,");         //采样率    如果采样率改变了，记得修改该值，只有16000、8000两个固定采样率
    strcat(data_json,"\"dev_pid\":1537,");      //中文普通话
    strcat(data_json,"\"channel\":1,");         //单声道
    strcat(data_json,"\"cuid\":\"123456\",");   //识别码    随便打几个字符，但最好唯一
   	strcat(data_json,"\"token\":\"25.e257a82b594ed459786800cef127cdd6.315360000.2023774405.282335-52321974\",");  //token		这里需要修改成自己申请到的token
    strcat(data_json,"\"len\":160000,");         //数据长度  如果传输的数据长度改变了，记得修改该值，该值是ADC采集的数据字节数，不是base64编码后的长度
    strcat(data_json,"\"speech\":\"");
    strcat(data_json,base64::encode((uint8_t *)rec_buffer,sizeof(rec_buffer)).c_str());     //base64编码数据   这里使用的base64编码的库，在base.h头文件中
    strcat(data_json,"\"");
    strcat(data_json,"}");
    Serial.println("Json_Success");


    free(rec_buffer); // 释放内存，避免内存泄漏
    Serial.println("Free_Success");

    
  

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

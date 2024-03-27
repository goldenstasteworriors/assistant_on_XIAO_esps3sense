#include  <WiFi.h>
#include  "WiFiClient.h"
#include  "Adafruit_MQTT.h"
#include  "Adafruit_MQTT_Client.h"

#define WLAN_SSID "ykj"
#define WLAN_PASS "604604604"

#define MQTT_SERVER "42.194.224.226"

#define MQTT_SERVERPORT 1883

#define MQTT_USERNAME "admin"
#define MQTT_KEY "13558030063ye"

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client,MQTT_SERVER,MQTT_SERVERPORT,MQTT_USERNAME,MQTT_KEY);
Adafruit_MQTT_Publish hello = Adafruit_MQTT_Publish(&mqtt, "hello");
Adafruit_MQTT_Subscribe test = Adafruit_MQTT_Subscribe(&mqtt, "test");


void setup() {
  Serial.begin(115200);
  WiFi.begin(WLAN_SSID,WLAN_PASS);
  delay(2000);
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);
  while(WiFi.status()!=WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.println("IP address:");
  Serial.println(WiFi.localIP());
}

void MQTT_connect()
{
  int8_t ret;
  if(mqtt.connected()){
    return;
  }
  Serial.print("Connecting to MQTT...");
  uint8_t retries=3;
  while((ret=mqtt.connect())!=0){
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
    retries--;
    if(retries==0){
      while(1);
    }

  }
  Serial.println("MQTT_Connected!");
}


void loop() {
  MQTT_connect();
  Serial.println(F("\nSending val"));
  if(!hello.publish("{\"msg\":\"hello from esp32\",\"err\":0}")){
    Serial.println(F("Failed"));
  }else{
    Serial.println("OK");
  }
  MQTT_connect();
  Adafruit_MQTT_Subscribe *subscription;
  // 在5s内判断是否有订阅消息进来
  while ((subscription = mqtt.readSubscription(5000))) 
  {
    Serial.println("subscription");
    // 判断是否是我们对应的主题
    if (subscription == &test)
    {
      Serial.print(F("Got msg of topic test: "));
      // 打印主题信息内容
      Serial.println((char *)test.lastread);
    }
  delay(2000);
}
}

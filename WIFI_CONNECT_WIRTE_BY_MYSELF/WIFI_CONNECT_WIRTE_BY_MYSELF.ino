
#include <WiFi.h>

const char* ssid="ChinaUnicom-844HJA";
const char* password="yesu13507738093";

void WiFiConnect(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);
  Serial.print("Connecting to: ");
  Serial.println(ssid);

  while(WiFi.status()!=WL_CONNECTED)
  {
    delay(1000);
    Serial.println("...");
  }

  Serial.println("");
  Serial.println("Connected to ");
  Serial.println(ssid);
  Serial.println("LocalIP: ");
  Serial.println(WiFi.localIP());

}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFiConnect();

}

void loop() {
  // put your main code here, to run repeatedly:

}

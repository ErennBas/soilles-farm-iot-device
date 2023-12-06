#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

String ssid, password;
int restartCounter = 0;
const int oneWireBus = 4;
const int led = 2;
long lastDataSendTime;
const int delayTime = 1000 * 60 * 10;

String uuid = "f12f55b7-edc3-460f-a472-8460cc2b959c";
String ip = "";
String mac = "";

WebServer server(80);
DHT dht(26, DHT11);
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
Preferences preferences;
DynamicJsonDocument configData(1024);

char webpage[] PROGMEM = R"=====(<html><head><style>body { display: flex; justify-content: center; align-items: center; font-size: 40px; font-family: monospace; }</style><head><body>I'm still running</body></html>)=====";
char updateWebPage[] PROGMEM = R"=====(<html lang="tr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>Document</title></head><body>Wifi Adı: <input type="text" id="ssid">parola: <input type="text" id="pw"><button onclick="send()">Kaydet</button><script>function send() {var data = JSON.stringify({"ssid": document.getElementById("ssid").value,"password": document.getElementById("pw").value});var xhr = new XMLHttpRequest();xhr.withCredentials = true;xhr.addEventListener("readystatechange", function () {if (this.readyState === 4) {console.log(this.responseText);}});xhr.open("POST", "/settings");xhr.setRequestHeader("Content-Type", "application/json");xhr.send(data);}</script></body></html>)=====";
struct IData
{
  float moisture;
  float weatherTemperature;
  float waterTemperature;
  String uuid;
};

void handleSettingsUpdate()
{
  String data = server.arg("plain");
  deserializeJson(configData, data);
  serializeJsonPretty(configData, Serial);
  preferences.putString("ssid", configData["ssid"].as<String>());
  preferences.putString("password", configData["password"].as<String>());
  Serial.println("-----data => " + data);
  server.send(200, "application/json", "{\"status\" : \"ok\"}");
  delay(1500);
  int i = 5;
  while (i > 0)
  {
    digitalWrite(led, HIGH);
    Serial.println("ESP Yeniden Başlatılıyor... " + i);
    i--;
    delay(500);
    digitalWrite(led, LOW);
    delay(500);
  }
  delay(500);
  preferences.end();
  ESP.restart();
}


void sendDevice()
{
  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();

  http.begin(client, "https://api.soilless-farming.erenbas.com/v1/device");
  http.addHeader("Content-Type", "application/json");
  String confTest = "{\"uuid\": \"" + uuid + "\", \"localIp\": \"" + ip + "\", \"macId\": \"" + mac + "\"}";
  int httpCode = http.POST(confTest);
  if (httpCode > 0)
  {
    String payload = http.getString();
    Serial.println("------PAYLOAD-----");
    Serial.println(payload);
    Serial.println("------PAYLOAD-----");
  }
}

void sendData(IData data)
{
  digitalWrite(led, HIGH);
  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();

  http.begin(client, "https://api.soilless-farming.erenbas.com/v1/data");
  http.addHeader("Content-Type", "application/json");
  String payload = "{\"uuid\": \"" + data.uuid + "\", \"waterTemperature\": " + String(data.waterTemperature) + ", \"weatherTemperature\": " + String(data.weatherTemperature) + ", \"moisture\": " + String(data.moisture) + "}";
  int httpCode = http.POST(payload);
  if (httpCode > 0)
  {
    String res = http.getString();
    Serial.println("------PAYLOAD-----");
    Serial.println(res);
    Serial.println("------PAYLOAD-----");
  }
  digitalWrite(led, LOW);
}

IData takeMeasurements()
{
  IData data;
  data.uuid = uuid;
  data.weatherTemperature = dht.readTemperature();
  data.moisture = dht.readHumidity();
  sensors.requestTemperatures();
  data.waterTemperature = sensors.getTempCByIndex(0);

  if (isnan(data.moisture))
  {
    data.moisture = -99;
  }
  if (isnan(data.weatherTemperature))
  {
    data.weatherTemperature = -99;
  }
  if (isnan(data.waterTemperature))
  {
    data.waterTemperature = -99;
  }

  Serial.println(data.moisture);
  Serial.println(data.weatherTemperature);
  Serial.println(data.waterTemperature);

  return data;
}

void setup()
{
  Serial.begin(115200);

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  pinMode(led, OUTPUT);

  preferences.begin("credentials", false);

  ssid = preferences.getString("ssid", "");
  Serial.println("111111");
  password = preferences.getString("password", "");
  Serial.println("222222");
  int counter = preferences.getInt("restartCounter", 0);
  Serial.println("333333");
  counter++;
  preferences.putInt("restartCounter", counter);
  Serial.println("444444");
  Serial.println(ssid);
  Serial.println(password);

  if (ssid == "" || password == "")
  {
    Serial.println("55555");
    IPAddress local_IP(192, 168, 0, 116);
    // Set your Gateway IP address
    IPAddress gateway(192, 168, 0, 1);

    IPAddress subnet(255, 255, 255, 0);
    Serial.println("8888");
    delay(500);
    Serial.println("9999");
    WiFi.mode(WIFI_AP);
    Serial.println("00000");
    delay(500);
    Serial.println("77777");
    WiFi.softAPConfig(local_IP, gateway, subnet);
    Serial.println("66666");
    // WiFi.softAP(mySsid, password, 1, true, 2); hidden
    WiFi.softAP("Soilless Farm IOT", "_sera123");
    Serial.println("wl connected true");
  }
  else
  {
    // Connect to Wi-Fi
    WiFi.mode(WIFI_STA);
    delay(500);
    unsigned long startTime = millis();
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED)
    {
      digitalWrite(led, HIGH);
      Serial.print('.');
      delay(500);
      digitalWrite(led, LOW);
      delay(500);
      if ((unsigned long)(millis() - startTime) >= 10000)
      {
        break;
      }
    }
    Serial.println(WiFi.localIP());
  }
  
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!MDNS.begin("soilless-farm-iot"))
    {
      Serial.println("Error starting mDNS");
      return;
    }
    else
    {
      Serial.println("Success starting mDNS");
      MDNS.addService("http", "tcp", 80);
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    mac = WiFi.macAddress();
    ip = WiFi.localIP().toString();

    sendDevice();

    dht.begin();
    delay(2000);
  }
  else {
    Serial.println("55555");
    IPAddress local_IP(192, 168, 0, 116);
    // Set your Gateway IP address
    IPAddress gateway(192, 168, 0, 1);

    IPAddress subnet(255, 255, 255, 0);
    Serial.println("8888");
    delay(500);
    Serial.println("9999");
    WiFi.mode(WIFI_AP);
    Serial.println("00000");
    delay(500);
    Serial.println("77777");
    WiFi.softAPConfig(local_IP, gateway, subnet);
    Serial.println("66666");
    // WiFi.softAP(mySsid, password, 1, true, 2); hidden
    WiFi.softAP("Soilless Farm IOT", "_sera123");
    Serial.println("wl connected true");
  }

  server.on("/", []()
            { server.send_P(200, "text/html", webpage); });
  server.on("/wifi", []()
            { server.send_P(200, "text/html", updateWebPage); });
  server.on("/settings", HTTP_POST, handleSettingsUpdate);
  server.begin();
}

void loop()
{
  server.handleClient();
  delay(2); // allow the cpu to switch to other tasks
  if (lastDataSendTime + delayTime < millis())
  {
    Serial.println("if içi");
    lastDataSendTime = millis();
    Serial.println("measur start");
    IData data = takeMeasurements();
    Serial.println("measur end");
    sendData(data);
  }
}

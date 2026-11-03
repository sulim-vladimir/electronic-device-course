#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_INA219.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

#define SDA_PIN 4
#define SCL_PIN 5
#define ONE_WIRE_BUS 14
#define BUTTON_PIN 16
#define LED_PIN 12 

const unsigned long SLEEP_TIME_SECONDS = 600; 

struct Config {
  char ssid[32];
  char password[32];
  char python_server[64];
  int python_port;
  char botToken[50];
  char chat_id[15];
  int marker; 
} settings;

ESP8266WebServer server(80);
DNSServer dnsServer;
Adafruit_BMP280 bmp;
Adafruit_INA219 ina219;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dallasSensors(&oneWire);

bool configMode = false;
bool ledState = LOW;
unsigned long lastLedTime = 0;

void loadConfig() {
  EEPROM.get(0, settings);
  if (settings.marker != 12345) {
    memset(&settings, 0, sizeof(Config));
    settings.python_port = 8000;
  }
}

void saveConfig() {
  settings.marker = 12345; 
  EEPROM.put(0, settings);
  EEPROM.commit();
}

void handleRoot() {
  String html = "<!DOCTYPE html><html lang='uk'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Weather Config</title>";
  html += "<style>body{background:#161719;color:#d8d9da;font-family:-apple-system,BlinkMacSystemFont,sans-serif;text-align:center;padding:10px;margin:0;}";
  html += ".box{background:#1f2226;border:1px solid #2a2c30;padding:20px;max-width:340px;margin:30px auto;border-radius:6px;box-shadow:0 4px 15px rgba(0,0,0,0.4);}";
  html += "h2{color:#fff;font-size:18px;margin-bottom:20px;text-transform:uppercase;letter-spacing:1px;}";
  html += "label{display:block;text-align:left;font-size:11px;color:#7b8087;text-transform:uppercase;font-weight:600;margin:10px 0 4px 5px;}";
  html += "input{display:block;margin-bottom:15px;padding:10px;width:100%;box-sizing:border-box;background:#161719;border:1px solid #2a2c30;color:#fff;border-radius:4px;font-size:14px;}";
  html += "input:focus{border-color:#4f92ff;outline:none;}";
  html += "button{background:#4f92ff;color:#fff;border:none;padding:12px;border-radius:4px;cursor:pointer;font-weight:600;font-size:14px;width:100%;margin-top:10px;text-transform:uppercase;}";
  html += "button:hover{background:#357ae8;}</style></head><body>";
  html += "<div class='box'><h2>Налаштування станції</h2><form action='/save' method='POST'>";
  html += "<label>Назва Wi-Fi мережі</label><input type='text' name='ssid' value='" + String(settings.ssid) + "' placeholder='Назва мережі'>";
  html += "<label>Пароль Wi-Fi</label><input type='password' name='password' value='" + String(settings.password) + "' placeholder='Пароль'>";
  html += "<label>IP адреса / Домен сайту</label><input type='text' name='server' value='" + String(settings.python_server) + "' placeholder='10.157.156.159'>";
  html += "<label>Порт сервера</label><input type='number' name='port' value='" + String(settings.python_port) + "'>";
  html += "<label>Telegram Bot Token</label><input type='text' name='token' value='" + String(settings.botToken) + "' placeholder='Token'>";
  html += "<label>Telegram Chat ID</label><input type='text' name='chat' value='" + String(settings.chat_id) + "' placeholder='Chat ID'>";
  html += "<button type='submit'>Зберегти параметри</button></form></div></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  memset(&settings, 0, sizeof(Config));
  strncpy(settings.ssid, server.arg("ssid").c_str(), 32);
  strncpy(settings.password, server.arg("password").c_str(), 32);
  strncpy(settings.python_server, server.arg("server").c_str(), 64);
  settings.python_port = server.arg("port").toInt();
  strncpy(settings.botToken, server.arg("token").c_str(), 50);
  strncpy(settings.chat_id, server.arg("chat").c_str(), 15);
  saveConfig();
  server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body style='background:#161719;color:#fff;font-family:sans-serif;text-align:center;padding:50px;'><h3>Параметри збережено!<br>Метеостанція перезавантажується...</h3></body></html>");
  delay(2000);
  ESP.restart();
}

void sendDataToPythonServer(float dallasTemp, float bmpTemp, float pressure, float vbat, float current, float power) {
  WiFiClient client;
  if (client.connect(settings.python_server, settings.python_port)) {
    String url = "/api/post-data/";
    String jsonPayload = "{\"temp_dallas\":" + String(dallasTemp, 1) + ",\"temp_bmp\":" + String(bmpTemp, 1) + ",\"pressure\":" + String(pressure, 1) + ",\"voltage\":" + String(vbat, 2) + ",\"current\":" + String(current, 1) + ",\"power\":" + String(power, 1) + "}";
    client.print(String("POST ") + url + " HTTP/1.1\r\nHost: " + String(settings.python_server) + ":" + String(settings.python_port) + "\r\nContent-Type: application/json\r\nContent-Length: " + String(jsonPayload.length()) + "\r\nConnection: close\r\n\r\n" + jsonPayload);
    client.stop();
  }
}

void sendTelegramMessage(String text) {
  WiFiClientSecure client;
  client.setInsecure(); 
  if (client.connect("api.telegram.org", 443)) {
    text.replace("\n", "%0A"); text.replace(" ", "%20");
    String url = "/bot" + String(settings.botToken) + "/sendMessage?chat_id=" + String(settings.chat_id) + "&text=" + text;
    client.print(String("GET ") + url + " HTTP/1.1\r\nHost: api.telegram.org\r\nConnection: close\r\n\r\n");
    client.stop();
  }
}

void readAndSendAllData() {
  dallasSensors.requestTemperatures();
  delay(750); 
  float dallasTemp = dallasSensors.getTempCByIndex(0);
  int attempts = 0;
  while ((dallasTemp == 85.0 || dallasTemp == -127.0) && attempts < 5) {
    delay(200); dallasSensors.requestTemperatures(); dallasTemp = dallasSensors.getTempCByIndex(0); attempts++;
  }
  float bmpTemp = bmp.readTemperature();
  float pressureMmHg = bmp.readPressure() * 0.00750062; 
  float busVoltage = ina219.getBusVoltage_V();
  float shuntVoltage = ina219.getShuntVoltage_mV() / 1000.0;
  float vbat = busVoltage + shuntVoltage;
  float currentMa = ina219.getCurrent_mA();
  float powerMw = ina219.getPower_mW();

  sendDataToPythonServer(dallasTemp, bmpTemp, pressureMmHg, vbat, currentMa, powerMw);
  
  String batteryStatus = (currentMa < 0) ? "Зарядка" : "Розрядка";
  String logMsg = " Дані:\n"
                  " Температура: " + String(dallasTemp, 1) + " °C\n"
                  " Температура HW: " + String(bmpTemp, 1) + " °C\n"
                  " Тиск: " + String(pressureMmHg, 1) + " мм\n"
                  " Напруга: " + String(vbat, 2) + " V\n"
                  " Струм панелі: " + String(currentMa, 1) + " mA (" + batteryStatus + ")\n"
                  " Потужність: " + String(powerMw, 1) + " mW";
  sendTelegramMessage(logMsg);
}

void startConfigPortal() {
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Meteo-Station-Config"); 
  delay(500);
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();
  configMode = true; 
  Serial.println("Портал конфігурації активовано.");
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  loadConfig();
  
  pinMode(BUTTON_PIN, INPUT); 
  pinMode(LED_PIN, OUTPUT); 
  digitalWrite(LED_PIN, LOW);

  
  rst_info *resetInfo = ESP.getResetInfoPtr();
  if (resetInfo->reason == REASON_EXT_SYS_RST || settings.marker != 12345 || settings.ssid[0] == '\0') {
    startConfigPortal();
    return;
  }

  Wire.begin(SDA_PIN, SCL_PIN);
  dallasSensors.begin(); bmp.begin(0x76); ina219.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(settings.ssid, settings.password);
  
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 30) {
    delay(500);
    wifiTimeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    readAndSendAllData();
    delay(1000); 
  }

  Serial.println("Цикл завершено. Сон на 10 хвилин...");
  ESP.deepSleep(SLEEP_TIME_SECONDS * 1000000); 
}

void loop() {
  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    if (millis() - lastLedTime >= 500) {
      lastLedTime = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  }
  delay(1);
}

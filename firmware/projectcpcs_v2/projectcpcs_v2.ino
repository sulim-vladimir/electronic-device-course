// Firmware for the v2 board (v2/last-version.kicad_sch).
//
// Pins on this board:
//   GPIO4  SDA, GPIO5 SCL      BMP280 (J2 / J5)
//   GPIO10 OneWire             DS18B20 (U3 / J4). Set Flash Mode to DIO or DOUT,
//                              in QIO mode GPIO10 belongs to the flash chip.
//   GPIO12 LED2, GPIO14 LED1
//   GPIO13 Mode jumper (SW3)   closed = config portal; R12 pulls it low when open
//   GPIO16                     wired to RESET through R14 for deep-sleep wake.
//                              Do not use it in code.

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

#define SDA_PIN 4
#define SCL_PIN 5
#define ONE_WIRE_BUS 10
#define MODE_PIN 13
#define LED_PIN 12

const uint64_t SLEEP_TIME_US = 600ULL * 1000000ULL;            // 10 minutes
const unsigned long CONFIG_TIMEOUT_MS = 5UL * 60UL * 1000UL;   // portal gives up after 5 minutes

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
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dallasSensors(&oneWire);

bool configMode = false;
bool bmpOk = false;
bool ledState = LOW;
unsigned long lastLedTime = 0;
unsigned long portalStartTime = 0;

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

void goToSleep() {
  digitalWrite(LED_PIN, LOW);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(10);
  Serial.println("Сон на 10 хвилин...");
  // GPIO16 pulls RESET low through R14 when the timer runs out.
  ESP.deepSleep(SLEEP_TIME_US);
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
  // sizeof - 1 keeps the final zero byte, so a max-length value is still a valid string
  strncpy(settings.ssid, server.arg("ssid").c_str(), sizeof(settings.ssid) - 1);
  strncpy(settings.password, server.arg("password").c_str(), sizeof(settings.password) - 1);
  strncpy(settings.python_server, server.arg("server").c_str(), sizeof(settings.python_server) - 1);
  settings.python_port = server.arg("port").toInt();
  strncpy(settings.botToken, server.arg("token").c_str(), sizeof(settings.botToken) - 1);
  strncpy(settings.chat_id, server.arg("chat").c_str(), sizeof(settings.chat_id) - 1);
  saveConfig();
  server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body style='background:#161719;color:#fff;font-family:sans-serif;text-align:center;padding:50px;'><h3>Параметри збережено!<br>Зніміть перемичку Mode.<br>Метеостанція перезавантажується...</h3></body></html>");
  delay(2000);
  ESP.restart();
}

void sendDataToPythonServer(float dallasTemp, float bmpTemp, float pressure) {
  WiFiClient client;
  if (client.connect(settings.python_server, settings.python_port)) {
    String url = "/api/post-data/";
    String jsonPayload = "{\"temp_dallas\":" + String(dallasTemp, 1) + ",\"temp_bmp\":" + String(bmpTemp, 1) + ",\"pressure\":" + String(pressure, 1) + "}";
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
  // requestTemperatures() waits for the conversion itself, no extra delay needed
  dallasSensors.requestTemperatures();
  float dallasTemp = dallasSensors.getTempCByIndex(0);
  int attempts = 0;
  while ((dallasTemp == 85.0 || dallasTemp == -127.0) && attempts < 5) {
    delay(200); dallasSensors.requestTemperatures(); dallasTemp = dallasSensors.getTempCByIndex(0); attempts++;
  }

  // Forced mode: the BMP280 takes one reading, then goes back to sleep on its own
  float bmpTemp = NAN;
  float pressureMmHg = NAN;
  if (bmpOk && bmp.takeForcedMeasurement()) {
    bmpTemp = bmp.readTemperature();
    pressureMmHg = bmp.readPressure() * 0.00750062;
  }

  sendDataToPythonServer(dallasTemp, bmpTemp, pressureMmHg);

  String logMsg = " Дані:\n"
                  " Температура: " + String(dallasTemp, 1) + " °C\n"
                  " Температура HW: " + String(bmpTemp, 1) + " °C\n"
                  " Тиск: " + String(pressureMmHg, 1) + " мм";
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
  portalStartTime = millis();
  Serial.println("Портал конфігурації активовано.");
}

void setup() {
  Serial.begin(115200);
  WiFi.persistent(false);  // credentials live in EEPROM, don't wear the SDK flash area
  EEPROM.begin(512);
  loadConfig();

  pinMode(MODE_PIN, INPUT);  // R12 pulls it low, the closed jumper pulls it high
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // A Reset press while the chip sleeps is reported as a deep-sleep wake,
  // so the Mode jumper is the reliable way in. A press while awake still works.
  rst_info *resetInfo = ESP.getResetInfoPtr();
  bool modeJumper = digitalRead(MODE_PIN) == HIGH;
  if (modeJumper || resetInfo->reason == REASON_EXT_SYS_RST || settings.marker != 12345 || settings.ssid[0] == '\0') {
    startConfigPortal();
    return;
  }

  Wire.begin(SDA_PIN, SCL_PIN);
  dallasSensors.begin();
  bmpOk = bmp.begin(0x76);
  if (bmpOk) {
    bmp.setSampling(Adafruit_BMP280::MODE_FORCED,
                    Adafruit_BMP280::SAMPLING_X2,     // temperature
                    Adafruit_BMP280::SAMPLING_X16,    // pressure
                    Adafruit_BMP280::FILTER_OFF,
                    Adafruit_BMP280::STANDBY_MS_1);
  }

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

  Serial.println("Цикл завершено.");
  goToSleep();
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

    // Don't drain the battery if nobody configures the station
    if (millis() - portalStartTime >= CONFIG_TIMEOUT_MS) {
      Serial.println("Портал: час вийшов.");
      goToSleep();
    }
  }
  delay(1);
}

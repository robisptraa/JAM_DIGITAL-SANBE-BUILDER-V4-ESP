// ====================
// JAM_DIGITAL_SANBE
// ====================

#include <SPI.h>
#include <WiFi.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <NTPClient.h>
#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> 
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Wire.h>
#include <RTClib.h>
#include <HTTPClient.h>

#define PANEL_RES_X 64      
#define PANEL_RES_Y 32     
#define PANEL_CHAIN 1      
#define W5500_SCK  32
#define W5500_MISO 22
#define W5500_MOSI 21
#define W5500_CS   33


char ssid[] = ""; 
char pass[] = ""; 
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

const char* apiEndpoint = "";

MatrixPanel_I2S_DMA *dma_display = nullptr;

WiFiUDP ntpUDP_WiFi;
EthernetUDP ntpUDP_Eth;
RTC_DS1307 rtc;

NTPClient timeClientWiFi(ntpUDP_WiFi, "pool.ntp.org", 7 * 3600);
NTPClient timeClientEth(ntpUDP_Eth, "pool.ntp.org", 7 * 3600);

bool isUsingLan = false;
bool isConnected = false;
bool rtcAvailable = false;
int h, m, d, month, yr;

uint16_t myWHITE, myBLACK, myRED, myGREEN;

void displaySimpleLayout() {
  dma_display->fillScreen(myBLACK);
  uint16_t borderColor = isConnected ? myGREEN : myRED;
  
  dma_display->drawRect(0, 0, PANEL_RES_X, PANEL_RES_Y, borderColor);     
  dma_display->drawRect(1, 1, PANEL_RES_X - 2, PANEL_RES_Y - 2, borderColor); 


  dma_display->setFont();      
  dma_display->setTextSize(2); 
  dma_display->setTextColor(myRED); 

  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", h, m); 

  dma_display->setCursor(3, 4); 
  dma_display->print(timeStr);

  dma_display->setTextSize(1); 
  dma_display->setTextColor(myRED); 

  char dateStr[12];
  sprintf(dateStr, "%02d/%02d/%02d", d, month + 1, yr % 100);

  int x_date = (PANEL_RES_X - 48) / 2; 
  dma_display->setCursor(x_date, 22);
  dma_display->print(dateStr);
}


// ================= CONNECTION =================
void checkConnection() {
  bool prevStatus = isConnected;

  if (isUsingLan) {
    isConnected = (Ethernet.linkStatus() == LinkON);
  } else {
    isConnected = (WiFi.status() == WL_CONNECTED);
  }


  if (prevStatus != isConnected) {
    Serial.print("[NETWORK] Status: ");
    Serial.println(isConnected ? "CONNECTED" : "DISCONNECTED");

    if (isUsingLan) {
      Serial.println("[NETWORK] Mode: LAN (W5500)");
    } else {
      Serial.println("[NETWORK] Mode: WiFi");
    }
  }
}

// ================= TIME =================
void updateTime() {
  bool gotInternetTime = false;

  // ===== LAN =====
  if (isUsingLan && Ethernet.linkStatus() == LinkON) {

    if (timeClientEth.update()) {

      h = timeClientEth.getHours();
      m = timeClientEth.getMinutes();

      time_t epochTime = timeClientEth.getEpochTime();
      struct tm *ptm = gmtime((time_t *)&epochTime);

      d = ptm->tm_mday;
      month = ptm->tm_mon;
      yr = ptm->tm_year + 1900;

      gotInternetTime = true;
    }

  }
  // ===== WIFI =====
  else if (WiFi.status() == WL_CONNECTED) {

    if (timeClientWiFi.update()) {

      h = timeClientWiFi.getHours();
      m = timeClientWiFi.getMinutes();

      time_t epochTime = timeClientWiFi.getEpochTime();
      struct tm *ptm = gmtime((time_t *)&epochTime);

      d = ptm->tm_mday;
      month = ptm->tm_mon;
      yr = ptm->tm_year + 1900;

      gotInternetTime = true;
    }
  }

  // ===== SAVE TO RTC =====
  if (gotInternetTime && rtcAvailable) {

    rtc.adjust(DateTime(yr, month + 1, d, h, m, 0));

    Serial.println("[RTC] Synced from NTP");
  }

  // ===== RTC FALLBACK =====
  if (!gotInternetTime && rtcAvailable) {

    DateTime now = rtc.now();

    h = now.hour();
    m = now.minute();

    d = now.day();
    month = now.month() - 1;
    yr = now.year();

    Serial.println("[RTC] Using DS1307 Backup Time");
  }

  // ===== SERIAL =====
  Serial.printf("[TIME] %02d:%02d | %02d/%02d/%04d\n",
                h, m, d, month + 1, yr);
}


// ================= POST API =================
void postDeviceStatus() {

  if (!isConnected) {
    Serial.println("[API] No internet connection");
    return;
  }

  HTTPClient http;

  String connectionType = isUsingLan ? "lan" : "wifi";

  String ipAddress;

  if (isUsingLan) {
    ipAddress = Ethernet.localIP().toString();
  } else {
    ipAddress = WiFi.localIP().toString();
  }

  // ===== MAC ADDRESS =====
  String macAddress;

  if (isUsingLan) {
    macAddress =
      String(mac[0], HEX) + ":" +
      String(mac[1], HEX) + ":" +
      String(mac[2], HEX) + ":" +
      String(mac[3], HEX) + ":" +
      String(mac[4], HEX) + ":" +
      String(mac[5], HEX);

  } else {
    macAddress = WiFi.macAddress();
  }

  macAddress.toUpperCase();

  // ===== UNIQUE DEVICE ID =====
  String deviceId = "clock-" + macAddress;
  deviceId.replace(":", "");

  // ===== DATE & TIME =====
  char clockStr[10];
  sprintf(clockStr, "%02d:%02d:00", h, m);

  char dateStr[20];
  sprintf(dateStr, "%04d-%02d-%02d", yr, month + 1, d);

  // ===== JSON =====
  String jsonPayload = "{";
  jsonPayload += "\"clock_device_id\":\"" + deviceId + "\",";
  jsonPayload += "\"actual_clock_device\":\"" + String(clockStr) + "\",";
  jsonPayload += "\"actual_date_device\":\"" + String(dateStr) + "\",";
  jsonPayload += "\"conection_clock_device_status\":\"";
  jsonPayload += (isConnected ? "connect" : "disconnect");
  jsonPayload += "\",";
  jsonPayload += "\"connection_type\":\"" + connectionType + "\",";
  jsonPayload += "\"mac_addres\":\"" + macAddress + "\",";
  jsonPayload += "\"ip_addres\":\"" + ipAddress + "\"";
  jsonPayload += "}";

  Serial.println("[API] POST DATA:");
  Serial.println(jsonPayload);

  http.begin(apiEndpoint);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonPayload);

  Serial.print("[API] Response Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = http.getString();

    Serial.println("[API] Response:");
    Serial.println(response);
  }

  http.end();
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  HUB75_I2S_CFG::i2s_pins pins = {
    25, 26, 27, 14, 12, 13,
    23, 19, 5, 17, 18, 4, 15, 16
  };

  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, pins);

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();

  myWHITE = dma_display->color565(255, 255, 255);
  myBLACK = dma_display->color565(0, 0, 0);
  myRED   = dma_display->color565(255, 0, 0);
  myGREEN = dma_display->color565(0, 255, 0);

  // ===== RTC INIT =====
Wire.begin(2, 0);
Wire.begin(0, 2);
 Wire.begin(21, 22);

if (rtc.begin()) {
  rtcAvailable = true;
  Serial.println("[RTC] DS1307 Ready");

  if (!rtc.isrunning()) {
    Serial.println("[RTC] RTC lost power, set default time");

    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

} else {
  Serial.println("[RTC] DS1307 NOT detected");
}

  // ===== LAN INIT =====
  SPI.begin(W5500_SCK, W5500_MISO, W5500_MOSI, W5500_CS);
  Ethernet.init(W5500_CS);

  Serial.println("[INIT] Checking LAN...");

  if (Ethernet.begin(mac) != 0 && Ethernet.linkStatus() == LinkON) {
    isUsingLan = true;
    timeClientEth.begin();

    Serial.println("[INIT] LAN Connected");

  Serial.print("[LAN] IP: ");
  Serial.println(Ethernet.localIP());

  Serial.print("[LAN] Gateway: ");
  Serial.println(Ethernet.gatewayIP());

  Serial.print("[LAN] DNS: ");
  Serial.println(Ethernet.dnsServerIP());
  } else {
    Serial.println("[INIT] LAN Failed , switching to WiFi");

    WiFi.begin(ssid, pass);

    Serial.print("[INIT] Connecting WiFi");
   unsigned long start = millis();
while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
  delay(50);
  Serial.print(".");
}
    Serial.println("\n[INIT] WiFi Connected ");
    timeClientWiFi.begin();
  }
}

void handleNetworkSwitch() {
  static bool wifiStarted = false;

  if (Ethernet.linkStatus() == LinkON) {

    if (!isUsingLan) {
      Serial.println("[SWITCH] Pindah ke LAN");

      isUsingLan = true;

      WiFi.disconnect(true);
      wifiStarted = false;

      timeClientEth.begin();
    }

  } else {

    if (!wifiStarted) {
      Serial.println("[SWITCH] LAN tidak ada, mulai WiFi...");

      WiFi.begin(ssid, pass);
      wifiStarted = true;
    }

    if (WiFi.status() == WL_CONNECTED) {

      if (isUsingLan || !isConnected) {
        Serial.println("[SWITCH] Sekarang pakai WiFi");

        isUsingLan = false;
        isConnected = true;

        timeClientWiFi.begin();
      }

    } else {
      isConnected = false;
    }
  }
}


// ================= LOOP =================
void loop() {
  static unsigned long lastUpdate = 0;

  handleNetworkSwitch();

 static unsigned long lastPost = 0;

  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();

    checkConnection();
    updateTime();
    displaySimpleLayout();

  if (millis() - lastPost >= 10000) {
  lastPost = millis();

  postDeviceStatus();
}
  }
}


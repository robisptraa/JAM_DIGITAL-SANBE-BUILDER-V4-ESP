// ====================
// JAM_DIGITAL_SANBE_DEV.4.5
// product_implement_PCB_Baru-no implement w5500
// author : obets
// project : jam digital panel p5 pt sanbe 
// ====================

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Wire.h>
#include <RTClib.h>

// panel_config
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

//WIFI_CONFIG
char ssid[] = "POCO M5";
char pass[] = "adielantp";

//API_CONFIG
const char* apiEndpoint = "";

//DISPLAY_CONFIG
MatrixPanel_I2S_DMA *dma_display = nullptr;
uint16_t myWHITE, myBLACK, myRED, myGREEN;

// RTC_config
RTC_DS1307 rtc;
bool rtcAvailable = false;

//NTP_config
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600);

//STATE_config
bool isConnected = false;
int h, m, d, month, yr;



// DISPLAY
void displayLayout() {
  dma_display->fillScreen(myBLACK);

  uint16_t borderColor = isConnected ? myGREEN : myRED;
  dma_display->drawRect(0, 0, PANEL_RES_X, PANEL_RES_Y, borderColor);
  dma_display->drawRect(1, 1, PANEL_RES_X - 2, PANEL_RES_Y - 2, borderColor);

  // jam_mapp
  dma_display->setFont();
  dma_display->setTextSize(2);
  dma_display->setTextColor(myRED);
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", h, m);
  dma_display->setCursor(3, 4);
  dma_display->print(timeStr);

  // tanggal_mapp
  dma_display->setTextSize(1);
  dma_display->setTextColor(myRED);
  char dateStr[16];
  sprintf(dateStr, "%02d/%02d/%02d", d, month + 1, yr % 100);
  int x_date = (PANEL_RES_X - 48) / 2;
  dma_display->setCursor(x_date, 22);
  dma_display->print(dateStr);
}


// WIFI_function

void checkConnection() {
  bool prev = isConnected;
  isConnected = (WiFi.status() == WL_CONNECTED);

  if (prev == isConnected) return;

  if (isConnected) {
    Serial.println("[WIFI] CONNECTED");
    Serial.print("[WIFI] IP : ");
    Serial.println(WiFi.localIP());
    timeClient.begin();
  } else {
    Serial.println("[WIFI] DISCONNECTED — fallback ke RTC");
    WiFi.disconnect();
    WiFi.begin(ssid, pass);
  }
}

// TIME,NTP, RTC fallback
void updateTime() {
  bool gotInternetTime = false;

  if (WiFi.status() == WL_CONNECTED) {
    bool ntpOK = timeClient.update();
    Serial.print("[NTP] update() : ");
    Serial.println(ntpOK ? "OK" : "GAGAL");

    if (ntpOK) {
      h = timeClient.getHours();
      m = timeClient.getMinutes();

      time_t epochTime = timeClient.getEpochTime();
      struct tm *ptm = gmtime(&epochTime);
      d     = ptm->tm_mday;
      month = ptm->tm_mon;
      yr    = ptm->tm_year + 1900;

      gotInternetTime = true;

      if (rtcAvailable) {
        rtc.adjust(DateTime(yr, month + 1, d, h, m, 0));
        Serial.println("[RTC] Sync from NTP");
      }
    }
  }

  // fallback RTC Failed
  if (!gotInternetTime) {
    if (rtcAvailable) {
      DateTime now = rtc.now();
      h     = now.hour();
      m     = now.minute();
      d     = now.day();
      month = now.month() - 1;
      yr    = now.year();
      Serial.println("[RTC] Using Backup RTC");
    } else {
      Serial.println("[TIME] WARNING — not connect update time");
    }
  }

  Serial.printf("[TIME] %02d:%02d | %02d/%02d/%04d\n",
                h, m, d, month + 1, yr);
}


// API POST function
void postDeviceStatus() {
  if (!isConnected) {
    Serial.println("[API] Skip — tidak ada koneksi");
    return;
  }

  if (String(apiEndpoint).length() == 0) {
    Serial.println("[API] Skip — apiEndpoint kosong");
    return;
  }

  String ipAddress  = WiFi.localIP().toString();
  String macAddress = WiFi.macAddress();
  macAddress.toUpperCase();

  String deviceId = "clock-" + macAddress;
  deviceId.replace(":", "");

  char clockStr[10], dateStr[20];
  sprintf(clockStr, "%02d:%02d:00",      h, m);
  sprintf(dateStr,  "%04d-%02d-%02d", yr, month + 1, d);

  String json = "{";
  json += "\"clock_device_id\":\""              + deviceId   + "\",";
  json += "\"actual_clock_device\":\""          + String(clockStr) + "\",";
  json += "\"actual_date_device\":\""           + String(dateStr)  + "\",";
  json += "\"conection_clock_device_status\":\"" + String(isConnected ? "connect" : "disconnect") + "\",";
  json += "\"connection_type\":\"wifi\",";
  json += "\"mac_addres\":\""                   + macAddress + "\",";
  json += "\"ip_addres\":\""                    + ipAddress  + "\"";
  json += "}";

  Serial.print("[API] POST : ");
  Serial.println(json);

  HTTPClient http;
  http.begin(apiEndpoint);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int code = http.POST(json);
  Serial.print("[API] Response : ");
  Serial.println(code);

  if (code < 0) {
    Serial.print("[API] Error    : ");
    Serial.println(http.errorToString(code));
  } else {
    Serial.print("[API] Body     : ");
    Serial.println(http.getString());
  }

  http.end();
}


// LOG PANEL P5 Mapper
void printPanelLog() {
  Serial.println("[PANEL] Panel P5 HUB75 Check");
  Serial.printf ("[PANEL] Resolution : %dx%d chain=%d\n",
                 PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  Serial.println("[PANEL] Pins : R1=19 G1=13 B1=18");
  Serial.println("[PANEL]        R2=5  G2=12 B2=17");
  Serial.println("[PANEL]        A=16  B=14  C=4  D=27  E=-1");
  Serial.println("[PANEL]        CLK=2 LAT=26 OE=15");

  if (dma_display == nullptr) {
    Serial.println("[PANEL] ERROR — dma_display null");
    return;
  }

  dma_display->fillScreen(dma_display->color565(255, 0, 0));
  Serial.println("[PANEL] RED   fill — 500ms");
  delay(500);

  dma_display->fillScreen(dma_display->color565(0, 255, 0));
  Serial.println("[PANEL] GREEN fill — 500ms");
  delay(500);

  dma_display->fillScreen(dma_display->color565(0, 0, 255));
  Serial.println("[PANEL] BLUE  fill — 500ms");
  delay(500);

  dma_display->fillScreen(dma_display->color565(0, 0, 0));
  Serial.println("[PANEL] Panel P5 OK");
}


// LOGGER WIFI non bloking
void printWifiLog() {
  Serial.println("[WIFI] Connection Check");
  Serial.print  ("[WIFI] SSID : ");
  Serial.println(ssid);
  Serial.print  ("[WIFI] Connecting");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 10000) {
      Serial.println();
      Serial.println("[WIFI] TIMEOUT 10s — lanjut tanpa internet");
      Serial.println("[WIFI] Jam akan pakai RTC backup");
      return; 
    }
    delay(300);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("[WIFI] CONNECTED");
  Serial.print  ("[WIFI] IP      : "); Serial.println(WiFi.localIP());
  Serial.print  ("[WIFI] MAC     : "); Serial.println(WiFi.macAddress());
  Serial.print  ("[WIFI] RSSI    : "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
  Serial.print  ("[WIFI] Gateway : "); Serial.println(WiFi.gatewayIP());
  Serial.print  ("[WIFI] DNS     : "); Serial.println(WiFi.dnsIP());
}




void setup() {
  Serial.begin(115200);

  HUB75_I2S_CFG::i2s_pins pins = {
    19, 13, 18,
    5,  12, 17,
    16, 14, 4, 27, -1,
    26, 15, 2
  };
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, pins);

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(128);

  myWHITE = dma_display->color565(255, 255, 255);
  myBLACK = dma_display->color565(0,   0,   0);
  myRED   = dma_display->color565(255, 0,   0);
  myGREEN = dma_display->color565(0,   255, 0);

  printPanelLog();

  Wire.begin(21, 22);
  if (rtc.begin()) {
    rtcAvailable = true;
    Serial.println("[RTC] DS1307 READY");
    if (!rtc.isrunning()) {
      Serial.println("[RTC] LOST POWER — set compile time");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  } else {
    Serial.println("[RTC] DS1307 NOT DETECTED");
  }

  WiFi.begin(ssid, pass);
  printWifiLog(); 

  if (WiFi.status() == WL_CONNECTED) {
    timeClient.begin();
  }
}



void loop() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastPost   = 0;

  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();

    checkConnection();   
    updateTime();       
    displayLayout();
  }

  if (millis() - lastPost >= 10000) {
    lastPost = millis();
    postDeviceStatus(); 
  }
}
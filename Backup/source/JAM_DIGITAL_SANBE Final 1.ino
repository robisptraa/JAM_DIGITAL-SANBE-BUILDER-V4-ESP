// ====================
// JAM_DIGITAL_SANBE_DEV.5.3
// otw final all flow(flash terakhir 23/05/2026)
// add trace mapping lan re map miso 
// author : obets
// project : jam digital panel p5 pt sanbe 
// ====================

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Wire.h>
#include <RTClib.h>


// P5 CONFIG 
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

// W5500 SPI PIN 
#define W5500_SCK  32
#define W5500_MISO 35 
#define W5500_MOSI 25
#define W5500_CS   23


// WIFI CONNECTION CONFIG 
char ssid[] = "Obet's";
char pass[] = "obets1234";

//LAN CONNECTION CONFIG 
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

//API SERVICE CONFIG 
const char* apiEndpoint = "";

//DISPLAY CONFIG 
MatrixPanel_I2S_DMA *dma_display = nullptr;
uint16_t myWHITE, myBLACK, myRED, myGREEN;

// RTC CONFIG
RTC_DS1307 rtc;
bool rtcAvailable = false;

//NTP CONFIG
WiFiUDP     ntpUDP_WiFi;
EthernetUDP ntpUDP_Eth;
NTPClient timeClientWiFi(ntpUDP_WiFi, "id.pool.ntp.org", 7 * 3600);
NTPClient timeClientEth (ntpUDP_Eth,  "id.pool.ntp.org", 7 * 3600);

unsigned long lastEpoch       = 0;
unsigned long lastEpochMillis = 0;

unsigned long lastNTPUpdate   = 0;
#define NTP_INTERVAL_MS 30000

// STATE NETWORK FLOW
bool isConnected = false;
bool isUsingLan  = false;
int  h, m, d, month, yr;

// MAC/IP HELPER 
bool lanLinkON() {
  return Ethernet.linkStatus() == LinkON;
}

String getMacString() {
  if (isUsingLan) {
    String s = "";
    for (int i = 0; i < 6; i++) {
      if (mac[i] < 0x10) s += "0";
      s += String(mac[i], HEX);
      if (i < 5) s += ":";
    }
    s.toUpperCase();
    return s;
  }
  String s = WiFi.macAddress();
  s.toUpperCase();
  return s;
}

String getIpString() {
  if (isUsingLan) {
    IPAddress ip = Ethernet.localIP();
    return String(ip[0])+"."+String(ip[1])+"."+String(ip[2])+"."+String(ip[3]);
  }
  return WiFi.localIP().toString();
}

void showSplash() {
  const char* line1 = "PT Sanbe";
  const char* line2 = "FARMA";

  const int charW = 6;
  const int charH = 8;

  // Hitung lebar text
  int textW1 = strlen(line1) * charW;
  int textW2 = strlen(line2) * charW;

  // Posisi tengah horizontal
  int x1 = (PANEL_RES_X - textW1) / 2;
  int x2 = (PANEL_RES_X - textW2) / 2;

  // Posisi vertikal 2 layer
  int totalH = (charH * 2) + 2; // 2 baris + jarak
  int startY = (PANEL_RES_Y - totalH) / 2;

  int y1 = startY;
  int y2 = startY + charH + 2;

  const int fadeSteps = 32;
  const int holdMs    = 2000;
  const int stepDelay = 18;

  auto drawFrame = [&](uint8_t brightness) {
    dma_display->setBrightness8(brightness);
    dma_display->fillScreen(myBLACK);

    dma_display->setFont();
    dma_display->setTextSize(1);


    dma_display->setTextColor(myRED);
    dma_display->setCursor(x1, y1);
    dma_display->print(line1);


    dma_display->setTextColor(myWHITE);
    dma_display->setCursor(x2, y2);
    dma_display->print(line2);
  };


  Serial.println("[SPLASH] Fade in...");
  for (int i = 0; i <= fadeSteps; i++) {
    uint8_t br = (uint8_t)((i * 200) / fadeSteps);
    drawFrame(br);
    delay(stepDelay);
  }


  Serial.println("[SPLASH] Hold...");
  delay(holdMs);


  Serial.println("[SPLASH] Fade out...");
  for (int i = fadeSteps; i >= 0; i--) {
    uint8_t br = (uint8_t)((i * 200) / fadeSteps);
    drawFrame(br);
    delay(stepDelay);
  }

  dma_display->setBrightness8(128);
  dma_display->fillScreen(myBLACK);

  Serial.println("[SPLASH] Done");
}

void displayLayout() {
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
  char dateStr[16];
  sprintf(dateStr, "%02d/%02d/%02d", d, month + 1, yr % 100);
  int x_date = (PANEL_RES_X - 48) / 2;
  dma_display->setCursor(x_date, 22);
  dma_display->print(dateStr);
}

void checkConnection() {
  bool prevConnected = isConnected;
  bool prevLan       = isUsingLan;

  if (lanLinkON()) {
    isUsingLan  = true;
    isConnected = true;
    if (!prevLan) {
      Serial.println("[NET] LAN terdeteksi — switch ke LAN");
      WiFi.disconnect(true);
      timeClientEth.begin();
    }
  } else {
    if (prevLan) {
      Serial.println("[NET] LAN putus — switch ke WiFi");
      isUsingLan = false;
      WiFi.begin(ssid, pass);
    }
    isConnected = (WiFi.status() == WL_CONNECTED);
    if (!prevConnected && isConnected) {
      timeClientWiFi.begin();
    }
  }

  if (prevConnected != isConnected || prevLan != isUsingLan) {
    Serial.print("[NET] Mode   : ");
    Serial.println(isUsingLan ? "LAN (W5500)" : "WiFi");
    Serial.print("[NET] Status : ");
    Serial.println(isConnected ? "CONNECTED" : "DISCONNECTED");
    if (isConnected) {
      Serial.print("[NET] IP  : "); Serial.println(getIpString());
      Serial.print("[NET] MAC : "); Serial.println(getMacString());
    } else {
      Serial.println("[NET] Fallback ke RTC / epoch lokal");
    }
  }
}

void updateTime() {
  bool gotTime = false;

  if (isConnected) {
    bool doForce = (lastNTPUpdate == 0) ||
                   (millis() - lastNTPUpdate >= NTP_INTERVAL_MS);

    if (doForce) {
      bool ntpOK = isUsingLan
                   ? timeClientEth.forceUpdate()
                   : timeClientWiFi.forceUpdate();

      Serial.print("[NTP] forceUpdate : ");
      Serial.print(isUsingLan ? "LAN" : "WiFi");
      Serial.print(" — ");
      Serial.println(ntpOK ? "OK" : "Failed");

      if (ntpOK) {
        NTPClient& active = isUsingLan ? timeClientEth : timeClientWiFi;
        lastEpoch         = active.getEpochTime();
        lastEpochMillis   = millis();
        lastNTPUpdate     = millis();

        if (rtcAvailable) {
          time_t e       = lastEpoch;
          struct tm *ptm = gmtime(&e);
          rtc.adjust(DateTime(ptm->tm_year + 1900, ptm->tm_mon + 1,
                              ptm->tm_mday, ptm->tm_hour,
                              ptm->tm_min,  ptm->tm_sec));
          Serial.println("[RTC] Sync from NTP");
        }
      }
    }

    if (lastEpoch > 0) {
      time_t now     = lastEpoch + (millis() - lastEpochMillis) / 1000;
      struct tm *ptm = gmtime(&now);
      h     = ptm->tm_hour;
      m     = ptm->tm_min;
      d     = ptm->tm_mday;
      month = ptm->tm_mon;
      yr    = ptm->tm_year + 1900;
      gotTime = true;
    }
  }

  if (!gotTime) {
    if (rtcAvailable) {
      DateTime now = rtc.now();
      h     = now.hour();
      m     = now.minute();
      d     = now.day();
      month = now.month() - 1;
      yr    = now.year();
      gotTime = true;
      Serial.println("[RTC] Using backup RTC");
    } else {
      Serial.println("[TIME] WARNING — Tidak ada sumber waktu");
    }
  }

  Serial.printf("[TIME] %02d:%02d | %02d/%02d/%04d\n",
                h, m, d, month + 1, yr);
}

//API Servce Method
void postDeviceStatus() {
  if (!isConnected) {
    Serial.println("[API] Skip — Not Connection ");
    return;
  }

  if (String(apiEndpoint).length() == 0) {
    Serial.println("[API] Skip — apiEndpoint Null");
    return;
  }

  String ipAddress  = getIpString();
  String macAddress = getMacString();

  String deviceId = "clock-" + macAddress;
  deviceId.replace(":", "");

  char clockStr[10], dateStr[20];
  sprintf(clockStr, "%02d:%02d:00",   h, m);
  sprintf(dateStr,  "%04d-%02d-%02d", yr, month + 1, d);

  String connType = isUsingLan ? "lan" : "wifi";

  String json = "{";
  json += "\"clock_device_id\":\""               + deviceId         + "\",";
  json += "\"actual_clock_device\":\""           + String(clockStr) + "\",";
  json += "\"actual_date_device\":\""            + String(dateStr)  + "\",";
  json += "\"conection_clock_device_status\":\"" + String(isConnected ? "connect" : "disconnect") + "\",";
  json += "\"connection_type\":\""               + connType         + "\",";
  json += "\"mac_addres\":\""                    + macAddress       + "\",";
  json += "\"ip_addres\":\""                     + ipAddress        + "\"";
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

void printPanelLog() {
  Serial.println("[PANEL] Panel P5 HUB75 Check");
  Serial.printf ("[PANEL] Resolution : %dx%d  chain=%d\n",
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

void printLanLog() {
  Serial.println("[LAN] W5500 Check");
  Serial.printf ("[LAN] SPI : SCK=%d MISO=%d MOSI=%d CS=%d\n",
                 W5500_SCK, W5500_MISO, W5500_MOSI, W5500_CS);
  Serial.print  ("[LAN] MAC : ");
  Serial.println(getMacString());
  Serial.println("[LAN] DHCP request...");

  if (Ethernet.begin(mac) != 0 && lanLinkON()) {
    isUsingLan  = true;
    isConnected = true;
    Serial.println("[LAN] CONNECTED — LAN sebagai network utama");
    Serial.print  ("[LAN] IP      : "); Serial.println(Ethernet.localIP());
    Serial.print  ("[LAN] Gateway : "); Serial.println(Ethernet.gatewayIP());
    Serial.print  ("[LAN] DNS     : "); Serial.println(Ethernet.dnsServerIP());
  } else {
    isUsingLan  = false;
    isConnected = false;
    Serial.println("[LAN] GAGAL — Cable no connect / W5500 not found");
    Serial.println("[LAN] Fallback to WiFi");
  }
}

void printWifiLog() {
  Serial.println("[WIFI] Connection Check");
  Serial.print  ("[WIFI] SSID    : "); Serial.println(ssid);
  Serial.print  ("[WIFI] Connecting");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 10000) {
      Serial.println();
      Serial.println("[WIFI] TIMEOUT 10s — next without internet");
      Serial.println("[WIFI/network] Device select RTC backup for time/date");
      return;
    }
    delay(300);
    Serial.print(".");
  }

  isConnected = true;
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

  showSplash();

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

  SPI.begin(W5500_SCK, W5500_MISO, W5500_MOSI, W5500_CS);

pinMode(W5500_CS, OUTPUT);
digitalWrite(W5500_CS, HIGH);
delay(100);

SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
digitalWrite(W5500_CS, LOW);
SPI.transfer(0x00);
SPI.transfer(0x39);
SPI.transfer(0x00);
uint8_t ver = SPI.transfer(0x00);
digitalWrite(W5500_CS, HIGH);
SPI.endTransaction();

Serial.print("[W5500] Version Register : 0x");
Serial.println(ver, HEX);
if (ver == 0x04) {
  Serial.println("[W5500] Chip TERDETEKSI ✓");
} else {
  Serial.println("[W5500] Chip TIDAK TERDETEKSI — cek wiring/pin");
}


Ethernet.init(W5500_CS);
SPI.setFrequency(8000000);
printLanLog();

  if (isUsingLan) {
    timeClientEth.begin();
  } else {
    WiFi.begin(ssid, pass);
    printWifiLog();
    if (isConnected) {
      timeClientWiFi.begin();
    }
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
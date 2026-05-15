#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#include <Wire.h>
#include <RTClib.h>

#include <PxMatrix.h>
#include <Ticker.h>

// ================= PANEL =================
#define P_LAT 4
#define P_A 23
#define P_B 19
#define P_C 5
#define P_D 17
#define P_OE 16

PxMATRIX display(
  64,
  32,
  P_LAT,
  P_OE,
  P_A,
  P_B,
  P_C,
  P_D
);

// ================= DISPLAY UPDATE =================
Ticker display_ticker;

void display_updater() {
  display.display(30);
}

// ================= WIFI =================
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// ================= RTC =================
RTC_DS1307 rtc;

// ================= NTP =================
WiFiUDP ntpUDP;

NTPClient timeClient(
  ntpUDP,
  "pool.ntp.org",
  7 * 3600
);

// ================= COLORS =================
uint16_t BLACK;
uint16_t WHITE;
uint16_t GREEN;
uint16_t RED;

// ================= TIME VAR =================
int h, m, s;
int d, mo, y;

bool wifiConnected = false;
bool rtcAvailable = false;

// ================= TIME =================
void updateTime() {

  wifiConnected =
    WiFi.status() == WL_CONNECTED;

  // ===== NTP =====
  if (wifiConnected) {

    if (timeClient.update()) {

      time_t epochTime =
        timeClient.getEpochTime();

      struct tm *ptm =
        gmtime((time_t *)&epochTime);

      h = ptm->tm_hour;
      m = ptm->tm_min;
      s = ptm->tm_sec;

      d = ptm->tm_mday;
      mo = ptm->tm_mon + 1;
      y = ptm->tm_year + 1900;

      // SAVE RTC
      if (rtcAvailable) {

        rtc.adjust(
          DateTime(
            y,
            mo,
            d,
            h,
            m,
            s
          )
        );
      }

      Serial.println("[NTP] SYNC");

      return;
    }
  }

  // ===== RTC FALLBACK =====
  if (rtcAvailable) {

    DateTime now = rtc.now();

    h = now.hour();
    m = now.minute();
    s = now.second();

    d = now.day();
    mo = now.month();
    y = now.year();

    Serial.println("[RTC] FALLBACK");
  }
}

// ================= DISPLAY =================
void drawDisplay() {

  display.clearDisplay();

  uint16_t borderColor =
    wifiConnected ? GREEN : RED;

  display.drawRect(
    0,
    0,
    64,
    32,
    borderColor
  );

  char timeStr[12];

  sprintf(
    timeStr,
    "%02d:%02d:%02d",
    h,
    m,
    s
  );

  display.setTextColor(WHITE);

  display.setCursor(4, 8);

  display.print(timeStr);

  char dateStr[20];

  sprintf(
    dateStr,
    "%02d/%02d/%04d",
    d,
    mo,
    y
  );

  display.setCursor(2, 20);

  display.print(dateStr);
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  Serial.println("BOOT OK");

  // ================= DISPLAY =================
  display.begin(8);

  display.setBrightness(40);

  display_ticker.attach_ms(
    2,
    display_updater
  );

  BLACK = display.color565(0, 0, 0);
  WHITE = display.color565(255, 255, 255);
  GREEN = display.color565(0, 255, 0);
  RED = display.color565(255, 0, 0);

  // ================= RTC =================
  Wire.begin(21, 22);

  if (rtc.begin()) {

    rtcAvailable = true;

    Serial.println("[RTC] READY");

    if (!rtc.isrunning()) {

      rtc.adjust(
        DateTime(
          F(__DATE__),
          F(__TIME__)
        )
      );

      Serial.println("[RTC] DEFAULT SET");
    }

  } else {

    Serial.println("[RTC] NOT FOUND");
  }

  // ================= WIFI =================
  Serial.println("[WIFI] CONNECTING");

  WiFi.begin(ssid, pass);

  unsigned long start = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 10000
  ) {

    delay(200);

    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {

    wifiConnected = true;

    Serial.println();
    Serial.println("[WIFI] CONNECTED");

    Serial.print("[IP] ");
    Serial.println(WiFi.localIP());

    timeClient.begin();

  } else {

    Serial.println();
    Serial.println("[WIFI] FAILED");
  }
}

// ================= LOOP =================
void loop() {

  updateTime();

  drawDisplay();

  Serial.printf(
    "%02d:%02d:%02d %02d/%02d/%04d\n",
    h,
    m,
    s,
    d,
    mo,
    y
  );

  delay(1000);
}
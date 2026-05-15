#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#include <Wire.h>
#include <RTClib.h>

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// ================= MATRIX =================
#define HARDWARE_TYPE MD_MAX72XX::PAROLA_HW
#define MAX_DEVICES 4

#define CLK_PIN   18
#define DATA_PIN  23
#define CS_PIN    5

MD_Parola display =
  MD_Parola(
    HARDWARE_TYPE,
    DATA_PIN,
    CLK_PIN,
    CS_PIN,
    MAX_DEVICES
  );

// ================= WIFI =================
char ssid[] = "";
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

// ================= VAR =================
bool rtcAvailable = false;

int h, m, s;
int d, mo, y;

// ================= TIME =================
void updateTime() {

  if (WiFi.status() == WL_CONNECTED) {

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
void showClock() {

  // ================= SIMULASI RTC =================
  h = 10;
  m = 45;
  s = 0;

  d = 24;
  mo = 4;
  y = 2024;

  // ================= TEXT JAM =================
  char timeBuf[10];

  sprintf(
    timeBuf,
    "%02d:%02d",
    h,
    m
  );

  // ================= TEXT TANGGAL =================
  char dateBuf[20];

  sprintf(
    dateBuf,
    "%02d/%02d/%02d",
    d,
    mo,
    y % 100
  );

  // ================= DISPLAY =================
  display.displayClear();

  // JAM BESAR
  display.displayText(
    timeBuf,
    PA_CENTER,
    20,
    0,
    PA_PRINT,
    PA_NO_EFFECT
  );

  while (!display.displayAnimate()) {
  }

  delay(500);

  // TANGGAL
  display.displayText(
    dateBuf,
    PA_CENTER,
    20,
    0,
    PA_PRINT,
    PA_NO_EFFECT
  );

  while (!display.displayAnimate()) {
  }

  // ================= SERIAL LOG RTC =================
  Serial.printf(
    "[RTC] %02d:%02d:%02d | %02d/%02d/%04d\n",
    h,
    m,
    s,
    d,
    mo,
    y
  );
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  Serial.println("BOOT OK");

  // ================= DISPLAY =================
  display.begin();

  display.setIntensity(1);

  display.displayClear();

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

  showClock();

  display.displayAnimate();

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
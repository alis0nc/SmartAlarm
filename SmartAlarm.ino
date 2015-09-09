// DS1307 RTC setup stuff
#include <Wire.h>
#include <RTClib.h>
// =====================================================
// lcd setup things
  #include <LCD.h>
  #include <LiquidCrystal_I2C.h>
  #define I2C_ADDR      0x27 // I2C address of PCF8574A
  #define BACKLIGHT_PIN 3
  #define En_pin        2
  #define Rw_pin        1
  #define Rs_pin        0
  #define D4_pin        4
  #define D5_pin        5
  #define D6_pin        6
  #define D7_pin        7
  LiquidCrystal_I2C lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin, BACKLIGHT_PIN, POSITIVE);
// end lcd setup things
// =====================================================
// cc3000 setup things
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed but DI

#define WLAN_SSID       "Anything Box"        // cannot be longer than 32 characters!
#define WLAN_PASS       "3l3ktr0delica"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define WEBSITE "www.adafruit.com"
#define WEBPAGE "/testwifi/index.html"
#define IDLE_TIMEOUT_MS 3000
// end cc3000 setup things
// =====================================================

DateTime now = DateTime(0);

/**
 * converts a three letter english abbreviation for a month into the months number
 */
uint8_t parseMonth (char *mo) {
  if (!strcmp(mo, "Jan")) {
    return 1;
  } else if (!strcmp(mo, "Feb")) {
    return 2;
  } else if (!strcmp(mo, "Mar")) {
    return 3;
  } else if (!strcmp(mo, "Apr")) {
    return 4;
  } else if (!strcmp(mo, "May")) {
    return 5;
  } else if (!strcmp(mo, "Jun")) {
    return 6;
  } else if (!strcmp(mo, "Jul")) {
    return 7;
  } else if (!strcmp(mo, "Aug")) {
    return 8;
  } else if (!strcmp(mo, "Sep")) {
    return 9;
  } else if (!strcmp(mo, "Oct")) {
    return 10;
  } else if (!strcmp(mo, "Nov")) {
    return 11;
  } else if (!strcmp(mo, "Dec")) {
    return 12;
  } else { // invalid
    return 0;
  }
}


/**
 * connects to a web server and parses its HTTP Date: header to get the current DateTime
 */
DateTime parseHeader(uint32_t ip, uint16_t port, char *host, char *path) {
  #define LINELEN 256
  /**
   * holds the HTTP Date header line, which fortunately has fixed with fields
   * Date: Tue 15 Nov 1994 08:12:31 GMT
   */
  typedef union dateline
  {
    char line[LINELEN];
    struct fields {
      char date[6];
      char dow[4]; char pad; 
      char dom[3]; char mmm[4]; char yyyy[5];
      char hh[3]; char mm[3]; char ss[3];
      char tz[4];
    } fields;
  } dateline;

  // first connect to the site we're gonna leech the datetime from
  Adafruit_CC3000_Client www = cc3000.connectTCP(ip, port);
  if (www.connected()) {
    www.fastrprint(F("GET "));
    www.fastrprint(path);
    www.fastrprint(F(" HTTP/1.1\r\n"));
    www.fastrprint(F("Host: ")); www.fastrprint(host); www.fastrprint(F("\r\n"));
    www.fastrprint(F("\r\n"));
    www.println();
  } else {
    Serial.println(F("Connection failed"));    
    return NULL;
  }

  // try to read the Date line in the http headers
  dateline l;
  size_t pos = 0;
  bool wehaveadate = 0;
  unsigned long lastRead = millis();
  while (www.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS)) {
    while (www.available()) {
      char c = www.read();
      if (c == '\n' || pos > LINELEN-2) { // we've reached a newline
        pos = 0;
        // now we need to see whether this is the 'Date:' line
        if (!strcmp(l.fields.date, "Date:")) { 
          wehaveadate = 1;
        }
      } else if (!wehaveadate) { // if we haven't reached a newline, put it in the buffer
        if (pos == 5 ||
            pos == 9 ||
            pos == 13||
            pos == 17||
            pos == 22||
            pos == 25||
            pos == 28||
            pos == 31) { // putting the nulls in the right place for field seperators
          l.line[pos] = 0;
        } else {
          l.line[pos] = c;
        }
        l.line[pos + 1] = 0;
        pos++;
      }
      lastRead = millis();
    }
  }
  www.close();
  if (wehaveadate) {
    return DateTime(atoi(l.fields.yyyy),
                    parseMonth(l.fields.mmm),
                    atoi(l.fields.dom),
                    atoi(l.fields.hh), 
                    atoi(l.fields.mm), 
                    atoi(l.fields.ss));    
  } else {
    return NULL;
  }
  
}

void setup() {
  // serial debug connection
  Serial.begin(115200);
  // put your setup code here, to run once:
  Serial.println(F("\nInitialising the LCD ..."));
  lcd.begin(20,4);
  lcd.home();
  lcd.print("Connecting...");
  lcd.setBacklight(HIGH);

  /* Initialise the module */
  Serial.println(F("\nInitialising the CC3000 ..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Unable to initialise the CC3000! Check your wiring?"));
    while(1);
  }
/* Attempt to connect to an access point */
  char *ssid = WLAN_SSID;             /* Max 32 chars */
  Serial.print(F("\nAttempting to connect to ")); Serial.println(ssid);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY, 5)) {
    Serial.println(F("Failed!"));
    while(1);
  }
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100);
  }  
  lcd.clear();

  // try to set current time
  uint32_t ip = 0;
  // Try looking up the website's IP address
  Serial.print(WEBSITE); Serial.print(F(" -> "));
  while (ip == 0) {
    if (! cc3000.getHostByName(WEBSITE, &ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
  }

  now = parseHeader(ip, 80, WEBSITE, WEBPAGE);
  Serial.print(F("year: "));
  Serial.println(now.year());
  Serial.print(F("month: "));
  Serial.println(now.month());
  Serial.print(F("day: "));
  Serial.println(now.day());
  Serial.print(F("hour: "));
  Serial.println(now.hour());
  Serial.print(F("minute: "));
  Serial.println(now.minute());
  Serial.print(F("second: "));
  Serial.println(now.second());
}

void loop() {
  // put your main code here, to run repeatedly:
  lcd.setCursor(0,0);
  if (cc3000.checkConnected()) {
    lcd.print("ssid: ");
    uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
    char ipbuf[17];
    cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv);
    lcd.setCursor(0,1);
    lcd.print(WLAN_SSID);
    lcd.setCursor(0,2);
    lcd.print("ip: ");
    sprintf(ipbuf, "%d.%d.%d.%d", (uint8_t)(ipAddress>>24), (uint8_t)(ipAddress>>16), (uint8_t)(ipAddress>>8), (uint8_t)(ipAddress));
    lcd.print(ipbuf);
  } else {
    lcd.print("disconnected!");
  }
 
  lcd.setCursor(0,3);
  lcd.print(now.year()); lcd.print('-');
  lcd.print(now.month()); lcd.print('-');
  lcd.print(now.day()); lcd.print(' ');
  lcd.print(now.hour()); lcd.print(':');
  lcd.print(now.minute()); lcd.print(':');
  lcd.print(now.second());
}

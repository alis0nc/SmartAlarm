// Headers
#include <Wire.h>
#include <RTClib.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"
#include <Client.h>
#include <aREST.h>
#include "Settings.h"
// DS1307 RTC setup stuff
RTC_DS1307 rtc;
// =====================================================
// lcd setup things
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
// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed but DI
// end cc3000 setup things
// =====================================================
// relay shield stuff
#define RELAY1 7
#define RELAY2 6
#define RELAY3 5 // don't use; conflicts with CC3000 VBAT_SW_EN
#define RELAY4 4
// keys
#define KEY_CANCEL 0
#define KEY_DOWN 1
#define KEY_UP 2
#define KEY_OK 8
// setting step enum
#define STEP_YEAR 0
#define STEP_MONTH 1
#define STEP_DAY 2
#define STEP_HOUR 3
#define STEP_MINUTE 4
#define STEP_SECOND 5
// random defines
#define NALARMS 10
#define LISTEN_PORT 80

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

typedef struct alarm {
  DateTime set;
  bool active;
  bool sounding;
  struct alarm *next;
} alarm;

/** 
 * raises an alarm
 */
class AlarmSounder {
  unsigned long PrevMillis;
  unsigned long DurationOn; // in millis
  unsigned long DurationOff; // in millis
  bool state;
  uint8_t pin;

public:
  AlarmSounder(unsigned long dOn, unsigned long dOff, uint8_t p) {
    DurationOn = dOn;
    DurationOff = dOff;
    pin = p;
    state = LOW;
  }

  AlarmSounder() { // default
    DurationOn = 1000; // 1 second
    DurationOff = 1000; // 1 second
    pin = RELAY1;
    state = LOW;
  }

  void PoundTheAlarm() { // shameless Nicki Minaj reference
    if (!state && (millis() - PrevMillis) > DurationOff) {
      // turn it on
      digitalWrite(pin, HIGH);
      state = HIGH;
      PrevMillis = millis();
    } else if (state && (millis() - PrevMillis) > DurationOn) {
      // turn it off
      digitalWrite(pin, LOW);
      state = LOW;
      PrevMillis = millis();
    }
  }

  void KillTheAlarm() {
    digitalWrite(pin, LOW);
    state = LOW;
    PrevMillis = 0L;
  }
};

/**
 * updates LCD
 * clock at 10Hz
 * wifi status every ten seconds
 */
class DisplayUpdater {
  // last updated
  unsigned long LastUpdatedTime;
  unsigned long LastUpdatedWifiStatus;
  const unsigned long TimeUpdateInterval = 100; // 100ms
  const unsigned long WifiUpdateInterval = 10000; // 10 seconds

public:
  DisplayUpdater() {
    LastUpdatedTime = 0;
    LastUpdatedWifiStatus = 0;
  }

  void update(LiquidCrystal_I2C& lcd, RTC_DS1307& rtc, Adafruit_CC3000& cc3000) {
    // is it time yet?
    if ((millis() - LastUpdatedTime) > TimeUpdateInterval) {
      char timedisplay[20];
      LastUpdatedTime = millis();
      now = rtc.now();
      sprintf(timedisplay, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
      lcd.setCursor(0,0);
      lcd.print(timedisplay);
    } else if ((millis() - LastUpdatedWifiStatus) > WifiUpdateInterval) {
      LastUpdatedWifiStatus = millis();
      lcd.setCursor(0,1);
      if (cc3000.checkConnected()) {
        lcd.print("ssid: ");
        uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
        char ipbuf[17];
        cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv);
        lcd.setCursor(0,2);
        lcd.print(WLAN_SSID);
        lcd.setCursor(0,3);
        lcd.print("ip: ");
        sprintf(ipbuf, "%d.%d.%d.%d", (uint8_t)(ipAddress>>24), (uint8_t)(ipAddress>>16), (uint8_t)(ipAddress>>8), (uint8_t)(ipAddress));
        lcd.print(ipbuf);
      } else {
        lcd.clear();
        lcd.print("disconnected!");
      }
    }
  }
};

/**
 * a queue of alarms
 */
class AlarmQueue {
  alarm *head;
  uint8_t size;
  uint8_t max_size;

public:
  AlarmQueue(uint8_t m) {
    max_size = m;
    head = NULL;
  }

  AlarmQueue() {
    max_size = NALARMS;
    head = NULL;
  }

  void push (alarm *a) {
    // special case for an empty list
    if (!head) {
      head = a;
      return;
    }
    alarm *cur = head;
    alarm *prev = NULL;
    while ( cur && (cur->set.unixtime() < a->set.unixtime())) {
      prev = cur;
      cur = cur->next;
    }
    if (prev) {
      prev->next = a;
    } else { // nothing came before the new alarm
      head = a;
    }
    a->next = cur;
  }

  alarm *pop() {
    // special case for an empty list
    if (!head) {
      return NULL;
    }
    alarm *tmp = head;
    head = head->next;
    return tmp;
  }

  alarm *peek() {
    return head;
  }
};


DisplayUpdater DU;
AlarmSounder AS(1000, 500, RELAY1);
AlarmQueue alarms(NALARMS);
aREST rest = aREST();
Adafruit_CC3000_Server restServer(LISTEN_PORT);

/**
 * REST compatible function for setting an alarm
 * yyyy-mm-dd_hh:mm:ss
 */
int setAlarm(String command) {
  Serial.println(command);
  // delimiters
  command[4] = 0; command[7] = 0; command[10] = 0;
  command[13] = 0; command[16] = 0;
  uint16_t y; uint8_t m; uint8_t d;
  uint8_t hh; uint8_t mm; uint8_t ss;
  y = atoi(command.c_str());
  m = atoi(command.c_str() + 5);
  d = atoi(command.c_str() + 8);
  hh = atoi(command.c_str() + 11);
  mm = atoi(command.c_str() + 14);
  ss = atoi(command.c_str() + 17);
  DateTime t = DateTime(y, m, d, hh, mm, ss);
  alarm *a;
  if (a = (alarm *)malloc(sizeof(alarm))) {
    a->set = t;
    a->active = TRUE;
    alarms.push(a);
    Serial.print("Setting alarm... ");
    Serial.println(t.unixtime());
    return 0; // success
  } else {
    return -1; // failure
  }
}

void setup() {
  // serial debug connection
  Serial.begin(115200);
  // I/O
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY4, OUTPUT);
  pinMode(KEY_CANCEL, INPUT_PULLUP);
  pinMode(KEY_DOWN, INPUT_PULLUP);
  pinMode(KEY_UP, INPUT_PULLUP);
  pinMode(KEY_OK, INPUT_PULLUP);
  // put your setup code here, to run once:
  Serial.println(F("\nInitialising the LCD ..."));
  lcd.begin(20,4);
  lcd.home();
  lcd.print("Connecting...");
  lcd.setBacklight(HIGH);

  // initialising the rtc
  rtc.begin();
  now = rtc.now(); // update the now

  // initialise the rest api  
  rest.function("setAlarm", setAlarm);
  rest.set_id("001");
  rest.set_name("SmartAlarm");


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
  Serial.println(F("Trying to set current time from website..."));
  uint8_t retry_count = 5;
  uint32_t ip = 0;
  // Try looking up the website's IP address
  while (ip == 0 && retry_count) {
    if (! cc3000.getHostByName(WEBSITE, &ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
    retry_count--;
  }
  if (ip) {
    DateTime cur = parseHeader(ip, 80, WEBSITE, WEBPAGE);
    cur = cur + TimeSpan(0, TZOFFSET, 0, 0);
    rtc.adjust(cur);
  } 

  // begin the rest server
  restServer.begin();
}

void loop() {
  static bool setting;
  static uint8_t setstep;
  static DateTime newalarm;
  // put your main code here, to run repeatedly:
  now = rtc.now();
  DU.update(lcd, rtc, cc3000);
  // handle rest calls
  Adafruit_CC3000_ClientRef client = restServer.available();
  rest.handle(client);
  // check alarms
  uint8_t i;
  if (alarms.peek() && alarms.peek()->set.unixtime() <= now.unixtime()) {
    // we have an active alarm!
    alarms.peek()->sounding = true;
    AS.PoundTheAlarm();
  }
  // do UI stuff
  if (!digitalRead(KEY_CANCEL)) {
    // cancel the alarm
    while(!digitalRead(KEY_CANCEL)); // debounce
    Serial.print("Cancelled!");
    alarm *cancelled = alarms.pop();
    cancelled->sounding = FALSE;
    free(cancelled);
    AS.KillTheAlarm();
  } else if (!setting && !digitalRead(KEY_OK)) {
    // enter the setting mode
    while(!digitalRead(KEY_OK)); // debouncing
    setting = true;
    newalarm = DateTime(now.year(), 
                        now.month(),
                        now.day(),
                        now.hour(),
                        now.minute(),
                        0);
    setstep = STEP_YEAR;
  } else if (setting) {
    switch (setstep) {
      case STEP_YEAR:
        if (!digitalRead(KEY_DOWN)) {
          while(!digitalRead(KEY_DOWN)); // debouncing
          newalarm = DateTime(newalarm.year() - 1, 
                              newalarm.month(), 
                              newalarm.day(),
                              newalarm.hour(),
                              newalarm.minute(),
                              newalarm.second());
        } else if (!digitalRead(KEY_UP)) {
          while(!digitalRead(KEY_UP)); // debouncing
          newalarm = DateTime(newalarm.year() + 1, 
                              newalarm.month(), 
                              newalarm.day(),
                              newalarm.hour(),
                              newalarm.minute(),
                              newalarm.second());
        } else if (!digitalRead(KEY_OK)) {
          while(!digitalRead(KEY_OK)); // debouncing
          setstep++;
        }
        break;
      case STEP_MONTH:
        if (!digitalRead(KEY_DOWN)) {
          while(!digitalRead(KEY_DOWN)); // debouncing
          newalarm = DateTime(newalarm.year(), 
                              newalarm.month() - 1, 
                              newalarm.day(),
                              newalarm.hour(),
                              newalarm.minute(),
                              newalarm.second());
        } else if (!digitalRead(KEY_UP)) {
          while(!digitalRead(KEY_UP)); // debouncing
          newalarm = DateTime(newalarm.year(), 
                              newalarm.month() + 1, 
                              newalarm.day(),
                              newalarm.hour(),
                              newalarm.minute(),
                              newalarm.second());
        } else if (!digitalRead(KEY_OK)) {
          while(!digitalRead(KEY_OK)); // debouncing
          setstep++;
        }
        break;
      case STEP_DAY:
        if (!digitalRead(KEY_DOWN)) {
          while(!digitalRead(KEY_DOWN)); // debouncing
          newalarm = DateTime(newalarm.year(), 
                              newalarm.month(),
                              newalarm.day() - 1,
                              newalarm.hour(),
                              newalarm.minute(),
                              newalarm.second());
        } else if (!digitalRead(KEY_UP)) {
          while(!digitalRead(KEY_UP)); // debouncing
          newalarm = DateTime(newalarm.year(), 
                              newalarm.month(), 
                              newalarm.day() + 1,
                              newalarm.hour(),
                              newalarm.minute(),
                              newalarm.second());
        } else if (!digitalRead(KEY_OK)) {
          while(!digitalRead(KEY_OK)); // debouncing
          setstep++;
        }
        break;
      case STEP_HOUR:
        if (!digitalRead(KEY_DOWN)) {
          while(!digitalRead(KEY_DOWN)); // debouncing
          newalarm = DateTime(newalarm.year(), 
                              newalarm.month(), 
                              newalarm.day(),
                              newalarm.hour() - 1,
                              newalarm.minute(),
                              newalarm.second());
        } else if (!digitalRead(KEY_UP)) {
          while(!digitalRead(KEY_UP)); // debouncing
          newalarm = DateTime(newalarm.year(), 
                              newalarm.month(), 
                              newalarm.day(),
                              newalarm.hour() + 1,
                              newalarm.minute(),
                              newalarm.second());
        } else if (!digitalRead(KEY_OK)) {
          while(!digitalRead(KEY_OK)); // debouncing
          setstep++;
        }
        break;
      case STEP_MINUTE:
        if (!digitalRead(KEY_DOWN)) {
          while(!digitalRead(KEY_DOWN)); // debouncing
          newalarm = DateTime(newalarm.year(), 
                              newalarm.month(), 
                              newalarm.day(),
                              newalarm.hour(),
                              newalarm.minute() - 1,
                              newalarm.second());
        } else if (!digitalRead(KEY_UP)) {
          while(!digitalRead(KEY_UP)); // debouncing
          newalarm = DateTime(newalarm.year(), 
                              newalarm.month(), 
                              newalarm.day(),
                              newalarm.hour(),
                              newalarm.minute() + 1,
                              newalarm.second());
        } else if (!digitalRead(KEY_OK)) {
          while(!digitalRead(KEY_OK)); // debouncing
          setstep++;
        }
        break;
      default: // end
        alarm *a;
        if (a = (alarm *)malloc(sizeof(alarm))) {
          a->set = newalarm;
          a->active = true;
          alarms.push(a);
        }
        setting = false;
        break;
    }
  }

}

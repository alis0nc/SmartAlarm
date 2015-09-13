// Settings.h

#ifndef _SETTINGS_h
#define _SETTINGS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

// settings
#define TZOFFSET        -4 // UTC-0400 eastern daylight time
#define WLAN_SSID       "Anything Box"        // cannot be longer than 32 characters!
#define WLAN_PASS       "3l3ktr0delica"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define WEBSITE "www.adafruit.com"
#define WEBPAGE "/testwifi/index.html"
#define IDLE_TIMEOUT_MS 3000

#endif


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

#define WEBSITE "www.adafruit.com"
#define WEBPAGE "/testwifi/index.html"
#define IDLE_TIMEOUT_MS 3000

#endif


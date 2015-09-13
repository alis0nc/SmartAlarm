// TembooAccount.h

#ifndef _TEMBOOACCOUNT_h
#define _TEMBOOACCOUNT_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

/*
IMPORTANT NOTE about TembooAccount.h

TembooAccount.h contains your Temboo account information and must be included
alongside your sketch. To do so, make a new tab in Arduino, call it TembooAccount.h,
and copy this content into it. 
*/

#define TEMBOO_ACCOUNT "dummy"  // Your Temboo account name 
#define TEMBOO_APP_KEY_NAME "dummy"  // Your Temboo app key name
#define TEMBOO_APP_KEY "dummy"  // Your Temboo app key

#define WLAN_SSID "dummy"
#define WLAN_PASS "dummy"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

/* 
The same TembooAccount.h file settings can be used for all Temboo SDK sketches.  
Keeping your account information in a separate file means you can share the 
main .ino file without worrying that you forgot to delete your credentials.
*/

#endif


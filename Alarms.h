// Alarms.h

#ifndef _ALARMS_h
#define _ALARMS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif
#include <RTClib.h>

#define NALARMS 10

typedef struct alarm {
  DateTime set;
  bool active;
  bool sounding;
  struct alarm *next;
} alarm;

/**
 * a queue of alarms
 */
class AlarmQueue {
  alarm *head;
  uint8_t size;
  uint8_t max_size;

public:
  AlarmQueue(uint8_t size);
  AlarmQueue();
  void push (alarm *a);
  alarm *pop();
  alarm *peek();
};

#endif


// 
// 
// 

#include "Alarms.h"

/**
 * AlarmQueue implementation
 */
AlarmQueue::AlarmQueue(uint8_t size) {
  max_size = size;
  head = NULL;
}

AlarmQueue::AlarmQueue() {
  max_size = NALARMS;
  head = NULL;
}

void AlarmQueue::push (alarm *a) {
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

alarm *AlarmQueue::pop() {
  // special case for an empty list
  if (!head) {
    return NULL;
  }
  alarm *tmp = head;
  head = head->next;
  return tmp;
}

alarm *AlarmQueue::peek() {
  return head;
}
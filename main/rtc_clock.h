#ifndef RTC_CLOCK_H
#define RTC_CLOCK_H

#include <time.h>

void RTC_Clock_init(void);
void RTC_Clock_setTime(struct tm time);
// void RTC_Clock_getTime(struct tm *time);
struct tm RTC_Clock_getTime(void);
void RTC_Clock_setTimeWithBuffer(const char *timestamp);

#endif
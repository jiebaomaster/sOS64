#ifndef __TIME_H__
#define __TIME_H__

// RTC（Real-Time Clock） 时间
struct time {
  int second; // 00
  int minute; // 02
  int hour;   // 04
  int day;    // 07
  int month;  // 08
  int year;   // 09+32
};

// 全局 RTC 时间
extern struct time time;

#define BCD2BIN(value) (((value)&0xf) + ((value) >> 4) * 10)

int get_cmos_time(struct time *time);

#endif
#include "TimeUtils.h"

#include <cstdio>
#include <cstring>

namespace akashvani {
namespace {

// Howard Hinnant's civil calendar algorithms, valid for the proleptic
// Gregorian calendar. This keeps formatting identical on host and ESP32.
int64_t daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned shiftedMonth =
      static_cast<unsigned>(static_cast<int>(month) + (month > 2 ? -3 : 9));
  const unsigned dayOfYear = (153u * shiftedMonth + 2u) / 5u + day - 1u;
  const unsigned dayOfEra =
      yearOfEra * 365u + yearOfEra / 4u - yearOfEra / 100u + dayOfYear;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(dayOfEra) - 719468;
}

void civilFromDays(int64_t days, int& year, unsigned& month, unsigned& day) {
  days += 719468;
  const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const unsigned dayOfEra = static_cast<unsigned>(days - era * 146097);
  const unsigned yearOfEra =
      (dayOfEra - dayOfEra / 1460u + dayOfEra / 36524u - dayOfEra / 146096u) / 365u;
  const unsigned dayOfYear =
      dayOfEra - (365u * yearOfEra + yearOfEra / 4u - yearOfEra / 100u);
  const unsigned monthPrime = (5u * dayOfYear + 2u) / 153u;
  const unsigned dayOfMonth = dayOfYear - (153u * monthPrime + 2u) / 5u + 1u;
  month = static_cast<unsigned>(static_cast<int>(monthPrime) + (monthPrime < 10u ? 3 : -9));
  year = static_cast<int>(yearOfEra) + static_cast<int>(era) * 400 + (month <= 2u ? 1 : 0);
  day = dayOfMonth;
}

bool parseTwoDigits(const char* text, int& value) {
  if (text[0] < '0' || text[0] > '9' || text[1] < '0' || text[1] > '9') {
    return false;
  }
  value = (text[0] - '0') * 10 + (text[1] - '0');
  return true;
}

bool parseFourDigits(const char* text, int& value) {
  if (text[0] < '0' || text[0] > '9' || text[1] < '0' || text[1] > '9' ||
      text[2] < '0' || text[2] > '9' || text[3] < '0' || text[3] > '9') {
    return false;
  }
  value = (text[0] - '0') * 1000 + (text[1] - '0') * 100 + (text[2] - '0') * 10 +
          (text[3] - '0');
  return true;
}

unsigned daysInMonth(int year, unsigned month) {
  static const uint8_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 0;
  }
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
    return 29;
  }
  return kDays[month - 1];
}

}  // namespace

bool epochToUtc(int64_t epochSeconds, UtcDateTime& out) {
  int64_t days = epochSeconds / 86400;
  int64_t secondsOfDay = epochSeconds % 86400;
  if (secondsOfDay < 0) {
    secondsOfDay += 86400;
    days -= 1;
  }
  int year = 1970;
  unsigned month = 1;
  unsigned day = 1;
  civilFromDays(days, year, month, day);
  out.year = year;
  out.month = static_cast<uint8_t>(month);
  out.day = static_cast<uint8_t>(day);
  out.hour = static_cast<uint8_t>(secondsOfDay / 3600);
  out.minute = static_cast<uint8_t>((secondsOfDay % 3600) / 60);
  out.second = static_cast<uint8_t>(secondsOfDay % 60);
  return true;
}

bool utcToEpoch(const UtcDateTime& value, int64_t& epochSeconds) {
  if (value.month < 1 || value.month > 12 || value.day < 1 ||
      value.day > daysInMonth(value.year, value.month) || value.hour > 23 ||
      value.minute > 59 || value.second > 59) {
    return false;
  }
  epochSeconds = daysFromCivil(value.year, value.month, value.day) * 86400 +
                 static_cast<int64_t>(value.hour) * 3600 +
                 static_cast<int64_t>(value.minute) * 60 + value.second;
  return true;
}

bool formatIso8601Utc(int64_t epochSeconds, char* output, size_t outputSize) {
  if (output == nullptr || outputSize < 21) {
    return false;
  }
  UtcDateTime value;
  if (!epochToUtc(epochSeconds, value)) {
    return false;
  }
  const int written = std::snprintf(output, outputSize, "%04d-%02u-%02uT%02u:%02u:%02uZ",
                                    value.year, value.month, value.day, value.hour,
                                    value.minute, value.second);
  return written == 20;
}

bool parseIso8601Utc(const char* text, int64_t& epochSeconds) {
  if (text == nullptr || std::strlen(text) != 20) {
    return false;
  }
  UtcDateTime value;
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  if (!parseFourDigits(text, year) || text[4] != '-' || !parseTwoDigits(text + 5, month) ||
      text[7] != '-' || !parseTwoDigits(text + 8, day) || text[10] != 'T' ||
      !parseTwoDigits(text + 11, hour) || text[13] != ':' ||
      !parseTwoDigits(text + 14, minute) || text[16] != ':' ||
      !parseTwoDigits(text + 17, second) || text[19] != 'Z') {
    return false;
  }
  value.year = year;
  value.month = static_cast<uint8_t>(month);
  value.day = static_cast<uint8_t>(day);
  value.hour = static_cast<uint8_t>(hour);
  value.minute = static_cast<uint8_t>(minute);
  value.second = static_cast<uint8_t>(second);
  return utcToEpoch(value, epochSeconds);
}

}  // namespace akashvani

//Date.library.js
// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

//15.9.1.2 Day Number and Time within Day
function Day(t) {
  return Math.floor(t/msPerDay);
}

function TimeWithinDay(t) {
  return t%msPerDay;
}

//15.9.1.3 Year Number
function DaysInYear(y){
  if(y%4 != 0) return 365;
  if(y%4 == 0 && y%100 != 0) return 366;
  if(y%100 == 0 && y%400 != 0) return 365;
  if(y%400 == 0) return 366;
}

function DayFromYear(y) {
  return (365*(y-1970)
          + Math.floor((y-1969)/4)
          - Math.floor((y-1901)/100)
          + Math.floor((y-1601)/400));
}

function TimeFromYear(y){
  return msPerDay*DayFromYear(y);
}

function YearFromTime(t) {
  t = Number(t);
  var sign = ( t < 0 ) ? -1 : 1;
  var year = ( sign < 0 ) ? 1969 : 1970;

  for(var time = 0;;year += sign){
    time = TimeFromYear(year);

    if(sign > 0 && time > t){
      year -= sign;
      break;
    }
    else if(sign < 0 && time <= t){
      break;
    }
  };
  return year;
}

function InLeapYear(t){
  if(DaysInYear(YearFromTime(t)) == 365)
    return 0;

  if(DaysInYear(YearFromTime(t)) == 366)
    return 1;
}

function DayWithinYear(t) {
  return Day(t)-DayFromYear(YearFromTime(t));
}

//15.9.1.4 Month Number
function MonthFromTime(t){
  var day = DayWithinYear(t);
  var leap = InLeapYear(t);

  if((0 <= day) && (day < 31)) return 0;
  if((31 <= day) && (day < (59+leap))) return 1;
  if(((59+leap) <= day) && (day < (90+leap))) return 2;
  if(((90+leap) <= day) && (day < (120+leap))) return 3;
  if(((120+leap) <= day) && (day < (151+leap))) return 4;
  if(((151+leap) <= day) && (day < (181+leap))) return 5;
  if(((181+leap) <= day) && (day < (212+leap))) return 6;
  if(((212+leap) <= day) && (day < (243+leap))) return 7;
  if(((243+leap) <= day) && (day < (273+leap))) return 8;
  if(((273+leap) <= day) && (day < (304+leap))) return 9;
  if(((304+leap) <= day) && (day < (334+leap))) return 10;
  if(((334+leap) <= day) && (day < (365+leap))) return 11;
}

//15.9.1.5 Date Number
function DateFromTime(t) {
  var day = DayWithinYear(t);
  var month = MonthFromTime(t);
  var leap = InLeapYear(t);

  if(month == 0) return day+1;
  if(month == 1) return day-30;
  if(month == 2) return day-58-leap;
  if(month == 3) return day-89-leap;
  if(month == 4) return day-119-leap;
  if(month == 5) return day-150-leap;
  if(month == 6) return day-180-leap;
  if(month == 7) return day-211-leap;
  if(month == 8) return day-242-leap;
  if(month == 9) return day-272-leap;
  if(month == 10) return day-303-leap;
  if(month == 11) return day-333-leap;
}

//15.9.1.6 Week Day
function WeekDay(t) {
  var weekday = (Day(t)+4)%7;
  return (weekday < 0 ? 7+weekday : weekday);
}

//15.9.1.9 Daylight Saving Time Adjustment
$LocalTZ = (new Date()).getTimezoneOffset() / -60;
if (DaylightSavingTA((new Date()).valueOf()) !== 0) {
   $LocalTZ -= 1;
}
var LocalTZA = $LocalTZ*msPerHour;

function DaysInMonth(m, leap) {
  m = m%12;

  //April, June, Sept, Nov
  if(m == 3 || m == 5 || m == 8 || m == 10 ) {
    return 30;
  }

  //Jan, March, May, July, Aug, Oct, Dec
  if(m == 0 || m == 2 || m == 4 || m == 6 || m == 7 || m == 9 || m == 11){
    return 31;
  }

  //Feb
  return 28+leap;
}

function GetSundayInMonth(t, m, count){
    var year = YearFromTime(t);
    var tempDate;

    if (count==='"first"') {
        for (var d=1; d <= DaysInMonth(m, InLeapYear(t)); d++) {
            tempDate = new Date(year, m, d);
            if (tempDate.getDay()===0) {
                return tempDate.valueOf();
            }
        }
    } else if(count==='"last"') {
        for (var d=DaysInMonth(m, InLeapYear(t)); d>0; d--) {
            tempDate = new Date(year, m, d);
            if (tempDate.getDay()===0) {
                return tempDate.valueOf();
            }
        }
    }
    throw new Error("Unsupported 'count' arg:" + count);
}
/*
function GetSundayInMonth(t, m, count){
  var year = YearFromTime(t);
  var leap = InLeapYear(t);
  var day = 0;

  if(m >= 1) day += DaysInMonth(0, leap);
  if(m >= 2) day += DaysInMonth(1, leap);
  if(m >= 3) day += DaysInMonth(2, leap);
  if(m >= 4) day += DaysInMonth(3, leap);
  if(m >= 5) day += DaysInMonth(4, leap);
  if(m >= 6) day += DaysInMonth(5, leap);
  if(m >= 7) day += DaysInMonth(6, leap);
  if(m >= 8) day += DaysInMonth(7, leap);
  if(m >= 9) day += DaysInMonth(8, leap);
  if(m >= 10) day += DaysInMonth(9, leap);
  if(m >= 11) day += DaysInMonth(10, leap);

  var month_start = TimeFromYear(year)+day*msPerDay;
  var sunday = 0;

  if(count === "last"){
    for(var last_sunday = month_start+DaysInMonth(m, leap)*msPerDay;
      WeekDay(last_sunday)>0;
      last_sunday -= msPerDay
    ){};
    sunday = last_sunday;
  }
  else {
    for(var first_sunday = month_start;
      WeekDay(first_sunday)>0;
      first_sunday += msPerDay
    ){};
    sunday = first_sunday+7*msPerDay*(count-1);
  }

  return sunday;
}*/

function DaylightSavingTA(t) {
//  t = t-LocalTZA;

  var DST_start = GetSundayInMonth(t, $DST_start_month, $DST_start_sunday) +
                  $DST_start_hour*msPerHour +
                  $DST_start_minutes*msPerMinute;

  var k = new Date(DST_start);

  var DST_end   = GetSundayInMonth(t, $DST_end_month, $DST_end_sunday) +
                  $DST_end_hour*msPerHour +
                  $DST_end_minutes*msPerMinute;

  if ( t >= DST_start && t < DST_end ) {
    return msPerHour;
  } else {
    return 0;
  }
}

//15.9.1.9 Local Time
function LocalTime(t){
  return t+LocalTZA+DaylightSavingTA(t);
}

function UTC(t) {
  return t-LocalTZA-DaylightSavingTA(t-LocalTZA);
}

//15.9.1.10 Hours, Minutes, Second, and Milliseconds
function HourFromTime(t){
  return Math.floor(t/msPerHour)%HoursPerDay;
}

function MinFromTime(t){
  return Math.floor(t/msPerMinute)%MinutesPerHour;
}

function SecFromTime(t){
  return Math.floor(t/msPerSecond)%SecondsPerMinute;
}

function msFromTime(t){
  return t%msPerSecond;
}

//15.9.1.11 MakeTime (hour, min, sec, ms)
function MakeTime(hour, min, sec, ms){
  if ( !isFinite(hour) || !isFinite(min) || !isFinite(sec) || !isFinite(ms)) {
    return Number.NaN;
  }

  hour = ToInteger(hour);
  min  = ToInteger(min);
  sec  = ToInteger(sec);
  ms   = ToInteger(ms);

  return ((hour*msPerHour) + (min*msPerMinute) + (sec*msPerSecond) + ms);
}

//15.9.1.12 MakeDay (year, month, date)
function MakeDay(year, month, date) {
  if ( !isFinite(year) || !isFinite(month) || !isFinite(date)) {
    return Number.NaN;
  }

  year = ToInteger(year);
  month = ToInteger(month);
  date = ToInteger(date );

  var result5 = year + Math.floor(month/12);
  var result6 = month%12;

  var sign = ( year < 1970 ) ? -1 : 1;
  var t =    ( year < 1970 ) ? 1 :  0;
  var y =    ( year < 1970 ) ? 1969 : 1970;

  if( sign == -1 ){
    for ( y = 1969; y >= year; y += sign ) {
      t += sign * DaysInYear(y)*msPerDay;
    }
  } else {
    for ( y = 1970 ; y < year; y += sign ) {
      t += sign * DaysInYear(y)*msPerDay;
    }
  }

  var leap = 0;
  for ( var m = 0; m < month; m++ ) {
    //if year is changed, than we need to recalculate leep
    leap = InLeapYear(t);
    t += DaysInMonth(m, leap)*msPerDay;
  }

  if ( YearFromTime(t) != result5 ) {
    return Number.NaN;
  }
  if ( MonthFromTime(t) != result6 ) {
    return Number.NaN;
  }
  if ( DateFromTime(t) != 1 ) {
    return Number.NaN;
  }

  return Day(t)+date-1;
}

//15.9.1.13 MakeDate (day, time)
function MakeDate( day, time ) {
  if(!isFinite(day) || !isFinite(time)) {
    return Number.NaN;
  }

  return day*msPerDay+time;
}

//15.9.1.14 TimeClip (time)
function TimeClip(time) {
  if(!isFinite(time) || Math.abs(time) > 8.64e15){
    return Number.NaN;
  }

  return ToInteger(time);
}

//Test Functions
//ConstructDate is considered deprecated, and should not be used directly from
//test262 tests as it's incredibly sensitive to DST start/end dates that 
//vary with geographic location.
function ConstructDate(year, month, date, hours, minutes, seconds, ms){
  /*
   * 1. Call ToNumber(year)
   * 2. Call ToNumber(month)
   * 3. If date is supplied use ToNumber(date); else use 1
   * 4. If hours is supplied use ToNumber(hours); else use 0
   * 5. If minutes is supplied use ToNumber(minutes); else use 0
   * 6. If seconds is supplied use ToNumber(seconds); else use 0
   * 7. If ms is supplied use ToNumber(ms); else use 0
   * 8. If Result(1) is not NaN and 0 <= ToInteger(Result(1)) <= 99, Result(8) is
   * 1900+ToInteger(Result(1)); otherwise, Result(8) is Result(1)
   * 9. Compute MakeDay(Result(8), Result(2), Result(3))
   * 10. Compute MakeTime(Result(4), Result(5), Result(6), Result(7))
   * 11. Compute MakeDate(Result(9), Result(10))
   * 12. Set the [[Value]] property of the newly constructed object to TimeClip(UTC(Result(11)))
   */
  var r1 = Number(year);
  var r2 = Number(month);
  var r3 = ((date && arguments.length > 2) ? Number(date) : 1);
  var r4 = ((hours && arguments.length > 3) ? Number(hours) : 0);
  var r5 = ((minutes && arguments.length > 4) ? Number(minutes) : 0);
  var r6 = ((seconds && arguments.length > 5) ? Number(seconds) : 0);
  var r7 = ((ms && arguments.length > 6) ? Number(ms) : 0);

  var r8 = r1;

  if(!isNaN(r1) && (0 <= ToInteger(r1)) && (ToInteger(r1) <= 99))
    r8 = 1900+r1;

  var r9 = MakeDay(r8, r2, r3);
  var r10 = MakeTime(r4, r5, r6, r7);
  var r11 = MakeDate(r9, r10);

  var retVal = TimeClip(UTC(r11));
  return retVal;
}



/**** Python code for initialize the above constants
// We may want to replicate the following in JavaScript.
// However, using JS date operations to generate parameters that are then used to
// test those some date operations seems unsound.  However, it isn't clear if there
//is a good interoperable alternative.

# Copyright 2009 the Sputnik authors.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

def GetDaylightSavingsTimes():
# Is the given floating-point time in DST?
def IsDst(t):
return time.localtime(t)[-1]
# Binary search to find an interval between the two times no greater than
# delta where DST switches, returning the midpoint.
def FindBetween(start, end, delta):
while end - start > delta:
middle = (end + start) / 2
if IsDst(middle) == IsDst(start):
start = middle
else:
end = middle
return (start + end) / 2
now = time.time()
one_month = (30 * 24 * 60 * 60)
# First find a date with different daylight savings.  To avoid corner cases
# we try four months before and after today.
after = now + 4 * one_month
before = now - 4 * one_month
if IsDst(now) == IsDst(before) and IsDst(now) == IsDst(after):
logger.warning("Was unable to determine DST info.")
return None
# Determine when the change occurs between now and the date we just found
# in a different DST.
if IsDst(now) != IsDst(before):
first = FindBetween(before, now, 1)
else:
first = FindBetween(now, after, 1)
# Determine when the change occurs between three and nine months from the
# first.
second = FindBetween(first + 3 * one_month, first + 9 * one_month, 1)
# Find out which switch is into and which if out of DST
if IsDst(first - 1) and not IsDst(first + 1):
start = second
end = first
else:
start = first
end = second
return (start, end)


def GetDaylightSavingsAttribs():
times = GetDaylightSavingsTimes()
if not times:
return None
(start, end) = times
def DstMonth(t):
return time.localtime(t)[1] - 1
def DstHour(t):
return time.localtime(t - 1)[3] + 1
def DstSunday(t):
if time.localtime(t)[2] > 15:
return "'last'"
else:
return "'first'"
def DstMinutes(t):
return (time.localtime(t - 1)[4] + 1) % 60
attribs = { }
attribs['start_month'] = DstMonth(start)
attribs['end_month'] = DstMonth(end)
attribs['start_sunday'] = DstSunday(start)
attribs['end_sunday'] = DstSunday(end)
attribs['start_hour'] = DstHour(start)
attribs['end_hour'] = DstHour(end)
attribs['start_minutes'] = DstMinutes(start)
attribs['end_minutes'] = DstMinutes(end)
return attribs

*********/

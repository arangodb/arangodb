/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

gTestsuite = 'ecma';

/*
 * Date functions used by tests in Date suite
 *
 */
var msPerDay =   86400000;
var HoursPerDay =  24;
var MinutesPerHour = 60;
var SecondsPerMinute = 60;
var msPerSecond =  1000;
var msPerMinute =  60000;  // msPerSecond * SecondsPerMinute
var msPerHour =   3600000; // msPerMinute * MinutesPerHour
var TZ_DIFF = getTimeZoneDiff();  // offset of tester's timezone from UTC
var TZ_ADJUST  = TZ_DIFF * msPerHour;
var TZ_PST = -8;  // offset of Pacific Standard Time from UTC
var PST_DIFF = TZ_DIFF - TZ_PST;  // offset of tester's timezone from PST
var PST_ADJUST = TZ_PST * msPerHour;
var TIME_0000  = (function ()
  { // calculate time for year 0
    for ( var time = 0, year = 1969; year >= 0; year-- ) {
      time -= TimeInYear(year);
    }
    return time;
  })();
var TIME_1970  = 0;
var TIME_2000  = 946684800000;
var TIME_1900  = -2208988800000;
var UTC_FEB_29_2000 = TIME_2000 + 31*msPerDay + 28*msPerDay;
var UTC_JAN_1_2005 = TIME_2000 + TimeInYear(2000) + TimeInYear(2001) +
  TimeInYear(2002) + TimeInYear(2003) + TimeInYear(2004);
var now = new Date();
var TIME_NOW = now.valueOf();  //valueOf() is to accurate to the millisecond
                               //Date.parse() is accurate only to the second

/*
 * Originally, the test suite used a hard-coded value TZ_DIFF = -8.
 * But that was only valid for testers in the Pacific Standard Time Zone!
 * We calculate the proper number dynamically for any tester. We just
 * have to be careful not to use a date subject to Daylight Savings Time...
 */
function getTimeZoneDiff()
{
  return -((new Date(2000, 1, 1)).getTimezoneOffset())/60;
}

/*
 * Date test "ResultArrays" are hard-coded for Pacific Standard Time.
 * We must adjust them for the tester's own timezone -
 */
function adjustResultArray(ResultArray, msMode)
{
  // If the tester's system clock is in PST, no need to continue -
//  if (!PST_DIFF) {return;}

  /* The date gTestcases instantiate Date objects in two different ways:
   *
   *        millisecond mode: e.g.   dt = new Date(10000000);
   *        year-month-day mode:  dt = new Date(2000, 5, 1, ...);
   *
   * In the first case, the date is measured from Time 0 in Greenwich (i.e. UTC).
   * In the second case, it is measured with reference to the tester's local timezone.
   *
   * In the first case we must correct those values expected for local measurements,
   * like dt.getHours() etc. No correction is necessary for dt.getUTCHours() etc.
   *
   * In the second case, it is exactly the other way around -
   */
  if (msMode)
  {
    // The hard-coded UTC milliseconds from Time 0 derives from a UTC date.
    // Shift to the right by the offset between UTC and the tester.
    var t = ResultArray[TIME]  +  TZ_DIFF*msPerHour;

    // Use our date arithmetic functions to determine the local hour, day, etc.
    ResultArray[HOURS] = HourFromTime(t);
    ResultArray[DAY] = WeekDay(t);
    ResultArray[DATE] = DateFromTime(t);
    ResultArray[MONTH] = MonthFromTime(t);
    ResultArray[YEAR] = YearFromTime(t); 
  }
  else
  {
    // The hard-coded UTC milliseconds from Time 0 derives from a PST date.
    // Shift to the left by the offset between PST and the tester.
    var t = ResultArray[TIME]  -  PST_DIFF*msPerHour;

    // Use our date arithmetic functions to determine the UTC hour, day, etc.
    ResultArray[TIME] = t;
    ResultArray[UTC_HOURS] = HourFromTime(t);
    ResultArray[UTC_DAY] = WeekDay(t);
    ResultArray[UTC_DATE] = DateFromTime(t);
    ResultArray[UTC_MONTH] = MonthFromTime(t);
    ResultArray[UTC_YEAR] = YearFromTime(t);
  }
}

function Day( t ) {
  return ( Math.floor(t/msPerDay ) );
}
function DaysInYear( y ) {
  if ( y % 4 != 0 ) {
    return 365;
  }
  if ( (y % 4 == 0) && (y % 100 != 0) ) {
    return 366;
  }
  if ( (y % 100 == 0) && (y % 400 != 0) ) {
    return 365;
  }
  if ( (y % 400 == 0) ){
    return 366;
  } else {
    return "ERROR: DaysInYear(" + y + ") case not covered";
  }
}
function TimeInYear( y ) {
  return ( DaysInYear(y) * msPerDay );
}
function DayNumber( t ) {
  return ( Math.floor( t / msPerDay ) );
}
function TimeWithinDay( t ) {

  var r = t % msPerDay;

  if (r < 0)
  {
    r += msPerDay;
  }
  return r;

}
function YearNumber( t ) {
}
function TimeFromYear( y ) {
  return ( msPerDay * DayFromYear(y) );
}
function DayFromYear( y ) {
  return ( 365*(y-1970) +
           Math.floor((y-1969)/4) -
           Math.floor((y-1901)/100) +
           Math.floor((y-1601)/400) );
}
function InLeapYear( t ) {
  if ( DaysInYear(YearFromTime(t)) == 365 ) {
    return 0;
  }
  if ( DaysInYear(YearFromTime(t)) == 366 ) {
    return 1;
  } else {
    return "ERROR:  InLeapYear("+ t + ") case not covered";
  }
}
function YearFromTime( t ) {
  t = Number( t );
  var sign = ( t < 0 ) ? -1 : 1;
  var year = ( sign < 0 ) ? 1969 : 1970;
  for ( var timeToTimeZero = t; ;  ) {
    // subtract the current year's time from the time that's left.
    timeToTimeZero -= sign * TimeInYear(year)

      // if there's less than the current year's worth of time left, then break.
      if ( sign < 0 ) {
        if ( sign * timeToTimeZero <= 0 ) {
          break;
        } else {
          year += sign;
        }
      } else {
        if ( sign * timeToTimeZero < 0 ) {
          break;
        } else {
          year += sign;
        }
      }
  }
  return ( year );
}
function MonthFromTime( t ) {
  // i know i could use switch but i'd rather not until it's part of ECMA
  var day = DayWithinYear( t );
  var leap = InLeapYear(t);

  if ( (0 <= day) && (day < 31) ) {
    return 0;
  }
  if ( (31 <= day) && (day < (59+leap)) ) {
    return 1;
  }
  if ( ((59+leap) <= day) && (day < (90+leap)) ) {
    return 2;
  }
  if ( ((90+leap) <= day) && (day < (120+leap)) ) {
    return 3;
  }
  if ( ((120+leap) <= day) && (day < (151+leap)) ) {
    return 4;
  }
  if ( ((151+leap) <= day) && (day < (181+leap)) ) {
    return 5;
  }
  if ( ((181+leap) <= day) && (day < (212+leap)) ) {
    return 6;
  }
  if ( ((212+leap) <= day) && (day < (243+leap)) ) {
    return 7;
  }
  if ( ((243+leap) <= day) && (day < (273+leap)) ) {
    return 8;
  }
  if ( ((273+leap) <= day) && (day < (304+leap)) ) {
    return 9;
  }
  if ( ((304+leap) <= day) && (day < (334+leap)) ) {
    return 10;
  }
  if ( ((334+leap) <= day) && (day < (365+leap)) ) {
    return 11;
  } else {
    return "ERROR: MonthFromTime("+t+") not known";
  }
}
function DayWithinYear( t ) {
  return( Day(t) - DayFromYear(YearFromTime(t)));
}
function DateFromTime( t ) {
  var day = DayWithinYear(t);
  var month = MonthFromTime(t);

  if ( month == 0 ) {
    return ( day + 1 );
  }
  if ( month == 1 ) {
    return ( day - 30 );
  }
  if ( month == 2 ) {
    return ( day - 58 - InLeapYear(t) );
  }
  if ( month == 3 ) {
    return ( day - 89 - InLeapYear(t));
  }
  if ( month == 4 ) {
    return ( day - 119 - InLeapYear(t));
  }
  if ( month == 5 ) {
    return ( day - 150- InLeapYear(t));
  }
  if ( month == 6 ) {
    return ( day - 180- InLeapYear(t));
  }
  if ( month == 7 ) {
    return ( day - 211- InLeapYear(t));
  }
  if ( month == 8 ) {
    return ( day - 242- InLeapYear(t));
  }
  if ( month == 9 ) {
    return ( day - 272- InLeapYear(t));
  }
  if ( month == 10 ) {
    return ( day - 303- InLeapYear(t));
  }
  if ( month == 11 ) {
    return ( day - 333- InLeapYear(t));
  }

  return ("ERROR:  DateFromTime("+t+") not known" );
}
function WeekDay( t ) {
  var weekday = (Day(t)+4) % 7;
  return( weekday < 0 ? 7 + weekday : weekday );
}

// missing daylight savings time adjustment

function HourFromTime( t ) {
  var h = Math.floor( t / msPerHour ) % HoursPerDay;
  return ( (h<0) ? HoursPerDay + h : h  );
}
function MinFromTime( t ) {
  var min = Math.floor( t / msPerMinute ) % MinutesPerHour;
  return( ( min < 0 ) ? MinutesPerHour + min : min  );
}
function SecFromTime( t ) {
  var sec = Math.floor( t / msPerSecond ) % SecondsPerMinute;
  return ( (sec < 0 ) ? SecondsPerMinute + sec : sec );
}
function msFromTime( t ) {
  var ms = t % msPerSecond;
  return ( (ms < 0 ) ? msPerSecond + ms : ms );
}
function LocalTZA() {
  return ( TZ_DIFF * msPerHour );
}
function UTC( t ) {
  return ( t - LocalTZA() - DaylightSavingTA(t - LocalTZA()) );
}
function LocalTime( t ) {
  return ( t + LocalTZA() + DaylightSavingTA(t) );
}
function DaylightSavingTA( t ) {
  t = t - LocalTZA();

  var dst_start = GetDSTStart(t);
  var dst_end   = GetDSTEnd(t);

  if ( t >= dst_start && t < dst_end )
    return msPerHour;

  return 0;
}

function GetFirstSundayInMonth( t, m ) {
  var year = YearFromTime(t);
  var leap = InLeapYear(t);

// month m 0..11
// april == 3
// march == 2

  // set time to first day of month m
  var time = TimeFromYear(year);
  for (var i = 0; i < m; ++i)
  {
    time += TimeInMonth(i, leap);
  }

  for ( var first_sunday = time; WeekDay(first_sunday) > 0;
        first_sunday += msPerDay )
  {
    ;
  }

  return first_sunday;
}

function GetLastSundayInMonth( t, m ) {
  var year = YearFromTime(t);
  var leap = InLeapYear(t);

// month m 0..11
// april == 3
// march == 2

  // first day of following month
  var time = TimeFromYear(year);
  for (var i = 0; i <= m; ++i)
  {
    time += TimeInMonth(i, leap);
  }
  // prev day == last day of month
  time -= msPerDay;

  for ( var last_sunday = time; WeekDay(last_sunday) > 0;
        last_sunday -= msPerDay )
  {
    ;
  }
  return last_sunday;
}

/*
  15.9.1.9 Daylight Saving Time Adjustment

  The implementation of ECMAScript should not try to determine whether
  the exact time was subject to daylight saving time, but just whether
  daylight saving time would have been in effect if the current
  daylight saving time algorithm had been used at the time. This avoids
  complications such as taking into account the years that the locale
  observed daylight saving time year round.
*/

/*
  US DST algorithm

  Before 2007, DST starts first Sunday in April at 2 AM and ends last
  Sunday in October at 2 AM

  Starting in 2007, DST starts second Sunday in March at 2 AM and ends
  first Sunday in November at 2 AM

  Note that different operating systems behave differently.

  Fully patched Windows XP uses the 2007 algorithm for all dates while
  fully patched Fedora Core 6 and RHEL 4 Linux use the algorithm in
  effect at the time.

  Since pre-2007 DST is a subset of 2007 DST rules, this only affects
  tests that occur in the period Mar-Apr and Oct-Nov where the two
  algorithms do not agree.

*/

function GetDSTStart( t )
{
  return (GetFirstSundayInMonth(t, 2) + 7*msPerDay + 2*msPerHour - LocalTZA());
}

function GetDSTEnd( t )
{
  return (GetFirstSundayInMonth(t, 10) + 2*msPerHour - LocalTZA());
}

function GetOldDSTStart( t )
{
  return (GetFirstSundayInMonth(t, 3) + 2*msPerHour - LocalTZA());
}

function GetOldDSTEnd( t )
{
  return (GetLastSundayInMonth(t, 9) + 2*msPerHour - LocalTZA());
}

function MakeTime( hour, min, sec, ms ) {
  if ( isNaN( hour ) || isNaN( min ) || isNaN( sec ) || isNaN( ms ) ) {
    return Number.NaN;
  }

  hour = ToInteger(hour);
  min  = ToInteger( min);
  sec  = ToInteger( sec);
  ms  = ToInteger( ms );

  return( (hour*msPerHour) + (min*msPerMinute) +
          (sec*msPerSecond) + ms );
}
function MakeDay( year, month, date ) {
  if ( isNaN(year) || isNaN(month) || isNaN(date) ) {
    return Number.NaN;
  }
  year = ToInteger(year);
  month = ToInteger(month);
  date = ToInteger(date );

  var sign = ( year < 1970 ) ? -1 : 1;
  var t =    ( year < 1970 ) ? 1 :  0;
  var y =    ( year < 1970 ) ? 1969 : 1970;

  var result5 = year + Math.floor( month/12 );
  var result6 = month % 12;

  if ( year < 1970 ) {
    for ( y = 1969; y >= year; y += sign ) {
      t += sign * TimeInYear(y);
    }
  } else {
    for ( y = 1970 ; y < year; y += sign ) {
      t += sign * TimeInYear(y);
    }
  }

  var leap = InLeapYear( t );

  for ( var m = 0; m < month; m++ ) {
    t += TimeInMonth( m, leap );
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

  return ( (Day(t)) + date - 1 );
}
function TimeInMonth( month, leap ) {
  // september april june november
  // jan 0  feb 1  mar 2 apr 3 may 4  june 5  jul 6
  // aug 7  sep 8  oct 9 nov 10 dec 11

  if ( month == 3 || month == 5 || month == 8 || month == 10 ) {
    return ( 30*msPerDay );
  }

  // all the rest
  if ( month == 0 || month == 2 || month == 4 || month == 6 ||
       month == 7 || month == 9 || month == 11 ) {
    return ( 31*msPerDay );
  }

  // save february
  return ( (leap == 0) ? 28*msPerDay : 29*msPerDay );
}
function MakeDate( day, time ) {
  if ( day == Number.POSITIVE_INFINITY ||
       day == Number.NEGATIVE_INFINITY ) {
    return Number.NaN;
  }
  if ( time == Number.POSITIVE_INFINITY ||
       time == Number.NEGATIVE_INFINITY ) {
    return Number.NaN;
  }
  return ( day * msPerDay ) + time;
}
function TimeClip( t ) {
  if ( isNaN( t ) ) {
    return ( Number.NaN );
  }
  if ( Math.abs( t ) > 8.64e15 ) {
    return ( Number.NaN );
  }

  return ( ToInteger( t ) );
}
function ToInteger( t ) {
  t = Number( t );

  if ( isNaN( t ) ){
    return ( Number.NaN );
  }
  if ( t == 0 || t == -0 ||
       t == Number.POSITIVE_INFINITY || t == Number.NEGATIVE_INFINITY ) {
    return 0;
  }

  var sign = ( t < 0 ) ? -1 : 1;

  return ( sign * Math.floor( Math.abs( t ) ) );
}
function Enumerate ( o ) {
  var p;
  for ( p in o ) {
    print( p +": " + o[p] );
  }
}

/* these functions are useful for running tests manually in Rhino */

function GetContext() {
  return Packages.com.netscape.javascript.Context.getCurrentContext();
}
function OptLevel( i ) {
  i = Number(i);
  var cx = GetContext();
  cx.setOptimizationLevel(i);
}
/* end of Rhino functions */


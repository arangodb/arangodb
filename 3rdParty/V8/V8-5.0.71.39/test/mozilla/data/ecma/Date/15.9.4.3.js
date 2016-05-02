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

gTestfile = '15.9.4.3.js';

var SECTION = "15.9.4.3";
var TITLE = "Date.UTC( year, month, date, hours, minutes, seconds, ms )";

// Dates around 1970

addNewTestCase( Date.UTC( 1970,0,1,0,0,0,0),
		"Date.UTC( 1970,0,1,0,0,0,0)",
		utc(1970,0,1,0,0,0,0) );

addNewTestCase( Date.UTC( 1969,11,31,23,59,59,999),
		"Date.UTC( 1969,11,31,23,59,59,999)",
		utc(1969,11,31,23,59,59,999) );
addNewTestCase( Date.UTC( 1972,1,29,23,59,59,999),
		"Date.UTC( 1972,1,29,23,59,59,999)",
		utc(1972,1,29,23,59,59,999) );
addNewTestCase( Date.UTC( 1972,2,1,23,59,59,999),
		"Date.UTC( 1972,2,1,23,59,59,999)",
		utc(1972,2,1,23,59,59,999) );
addNewTestCase( Date.UTC( 1968,1,29,23,59,59,999),
		"Date.UTC( 1968,1,29,23,59,59,999)",
		utc(1968,1,29,23,59,59,999) );
addNewTestCase( Date.UTC( 1968,2,1,23,59,59,999),
		"Date.UTC( 1968,2,1,23,59,59,999)",
		utc(1968,2,1,23,59,59,999) );
addNewTestCase( Date.UTC( 1969,0,1,0,0,0,0),
		"Date.UTC( 1969,0,1,0,0,0,0)",
		utc(1969,0,1,0,0,0,0) );
addNewTestCase( Date.UTC( 1969,11,31,23,59,59,1000),
		"Date.UTC( 1969,11,31,23,59,59,1000)",
		utc(1970,0,1,0,0,0,0) );
addNewTestCase( Date.UTC( 1969,Number.NaN,31,23,59,59,999),
		"Date.UTC( 1969,Number.NaN,31,23,59,59,999)",
		utc(1969,Number.NaN,31,23,59,59,999) );

// Dates around 2000

addNewTestCase( Date.UTC( 1999,11,31,23,59,59,999),
		"Date.UTC( 1999,11,31,23,59,59,999)",
		utc(1999,11,31,23,59,59,999) );
addNewTestCase( Date.UTC( 2000,0,1,0,0,0,0),
		"Date.UTC( 2000,0,1,0,0,0,0)",
		utc(2000,0,1,0,0,0,0) );

// Dates around 1900
addNewTestCase( Date.UTC( 1899,11,31,23,59,59,999),
		"Date.UTC( 1899,11,31,23,59,59,999)",
		utc(1899,11,31,23,59,59,999) );
addNewTestCase( Date.UTC( 1900,0,1,0,0,0,0),
		"Date.UTC( 1900,0,1,0,0,0,0)",
		utc(1900,0,1,0,0,0,0) );
addNewTestCase( Date.UTC( 1973,0,1,0,0,0,0),
		"Date.UTC( 1973,0,1,0,0,0,0)",
		utc(1973,0,1,0,0,0,0) );
addNewTestCase( Date.UTC( 1776,6,4,12,36,13,111),
		"Date.UTC( 1776,6,4,12,36,13,111)",
		utc(1776,6,4,12,36,13,111) );
addNewTestCase( Date.UTC( 2525,9,18,15,30,1,123),
		"Date.UTC( 2525,9,18,15,30,1,123)",
		utc(2525,9,18,15,30,1,123) );

// Dates around 29 Feb 2000

addNewTestCase( Date.UTC( 2000,1,29,0,0,0,0 ),
		"Date.UTC( 2000,1,29,0,0,0,0 )",
		utc(2000,1,29,0,0,0,0) );
addNewTestCase( Date.UTC( 2000,1,29,8,0,0,0 ),
		"Date.UTC( 2000,1,29,8,0,0,0 )",
		utc(2000,1,29,8,0,0,0) );

// Dates around 1 Jan 2005

addNewTestCase( Date.UTC( 2005,0,1,0,0,0,0 ),
		"Date.UTC( 2005,0,1,0,0,0,0 )",
		utc(2005,0,1,0,0,0,0) );
addNewTestCase( Date.UTC( 2004,11,31,16,0,0,0 ),
		"Date.UTC( 2004,11,31,16,0,0,0 )",
		utc(2004,11,31,16,0,0,0) );

test();

function addNewTestCase( DateCase, DateString, ExpectDate) {
  DateCase = DateCase;

  new TestCase( SECTION, DateString,         ExpectDate.value,       DateCase );
  new TestCase( SECTION, DateString,         ExpectDate.value,       DateCase );
}

function MyDate() {
  this.year = 0;
  this.month = 0;
  this.date = 0;
  this.hours = 0;
  this.minutes = 0;
  this.seconds = 0;
  this.ms = 0;
}

function utc( year, month, date, hours, minutes, seconds, ms ) {
  d = new MyDate();
  d.year      = Number(year);

  if (month)
    d.month     = Number(month);
  if (date)
    d.date      = Number(date);
  if (hours)
    d.hours     = Number(hours);
  if (minutes)
    d.minutes   = Number(minutes);
  if (seconds)
    d.seconds   = Number(seconds);
  if (ms)
    d.ms        = Number(ms);

  if ( isNaN(d.year) && 0 <= ToInteger(d.year) && d.year <= 99 ) {
    d.year = 1900 + ToInteger(d.year);
  }

  if (isNaN(month) || isNaN(year) || isNaN(date) || isNaN(hours) ||
      isNaN(minutes) || isNaN(seconds) || isNaN(ms) ) {
    d.year = Number.NaN;
    d.month = Number.NaN;
    d.date = Number.NaN;
    d.hours = Number.NaN;
    d.minutes = Number.NaN;
    d.seconds = Number.NaN;
    d.ms = Number.NaN;
    d.value = Number.NaN;
    d.time = Number.NaN;
    d.day =Number.NaN;
    return d;
  }

  d.day = MakeDay( d.year, d.month, d.date );
  d.time = MakeTime( d.hours, d.minutes, d.seconds, d.ms );
  d.value = (TimeClip( MakeDate(d.day,d.time)));

  return d;
}

function UTCTime( t ) {
  sign = ( t < 0 ) ? -1 : 1;
  return ( (t +(TZ_DIFF*msPerHour)) );
}


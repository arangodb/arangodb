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

gTestfile = '15.9.3.1-5.js';

/**
   File Name:          15.9.3.1.js
   ECMA Section:       15.9.3.1 new Date (year, month, date, hours, minutes, seconds, ms)
   Description:        The [[Prototype]] property of the newly constructed
   object is set to the original Date prototype object,
   the one that is the initial value of Date.prototype.

   The [[Class]] property of the newly constructed object
   is set as follows:
   1. Call ToNumber(year)
   2. Call ToNumber(month)
   3. Call ToNumber(date)
   4. Call ToNumber(hours)
   5. Call ToNumber(minutes)
   6. Call ToNumber(seconds)
   7. Call ToNumber(ms)
   8.  If Result(1) is NaN and 0 <= ToInteger(Result(1)) <=
   99, Result(8) is 1900+ToInteger(Result(1)); otherwise,
   Result(8) is Result(1)
   9.  Compute MakeDay(Result(8), Result(2), Result(3)
   10. Compute MakeTime(Result(4), Result(5), Result(6),
   Result(7)
   11. Compute MakeDate(Result(9), Result(10))
   12. Set the [[Value]] property of the newly constructed
   object to TimeClip(UTC(Result(11))).


   This tests the returned value of a newly constructed
   Date object.

   Author:             christine@netscape.com
   Date:               7 july 1997
*/

var SECTION = "15.9.3.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "new Date( year, month, date, hours, minutes, seconds, ms )";

var TIME        = 0;
var UTC_YEAR    = 1;
var UTC_MONTH   = 2;
var UTC_DATE    = 3;
var UTC_DAY     = 4;
var UTC_HOURS   = 5;
var UTC_MINUTES = 6;
var UTC_SECONDS = 7;
var UTC_MS      = 8;

var YEAR        = 9;
var MONTH       = 10;
var DATE        = 11;
var DAY         = 12;
var HOURS       = 13;
var MINUTES     = 14;
var SECONDS     = 15;
var MS          = 16;

writeHeaderToLog( SECTION + " "+ TITLE);

// all the "ResultArrays" below are hard-coded to Pacific Standard Time values -

// Dates around 2005

addNewTestCase( new Date(2005,0,1,0,0,0,0),
		"new Date(2005,0,1,0,0,0,0)",
		[UTC_JAN_1_2005-PST_ADJUST,2005,0,1,6,8,0,0,0,2005,0,1,6,0,0,0,0] );

addNewTestCase( new Date(2004,11,31,16,0,0,0),
		"new Date(2004,11,31,16,0,0,0)",
		[UTC_JAN_1_2005,2005,0,1,6,0,0,0,0,2004,11,31,5,16,0,0,0] );

test();

function addNewTestCase( DateCase, DateString, ResultArray ) {
  //adjust hard-coded ResultArray for tester's timezone instead of PST
  adjustResultArray(ResultArray);

  new TestCase( SECTION, DateString+".getTime()", ResultArray[TIME],       DateCase.getTime() );
  new TestCase( SECTION, DateString+".valueOf()", ResultArray[TIME],       DateCase.valueOf() );

  new TestCase( SECTION, DateString+".getUTCFullYear()",      ResultArray[UTC_YEAR],  DateCase.getUTCFullYear() );
  new TestCase( SECTION, DateString+".getUTCMonth()",         ResultArray[UTC_MONTH],  DateCase.getUTCMonth() );
  new TestCase( SECTION, DateString+".getUTCDate()",          ResultArray[UTC_DATE],   DateCase.getUTCDate() );
  new TestCase( SECTION, DateString+".getUTCDay()",           ResultArray[UTC_DAY],    DateCase.getUTCDay() );
  new TestCase( SECTION, DateString+".getUTCHours()",         ResultArray[UTC_HOURS],  DateCase.getUTCHours() );
  new TestCase( SECTION, DateString+".getUTCMinutes()",       ResultArray[UTC_MINUTES],DateCase.getUTCMinutes() );
  new TestCase( SECTION, DateString+".getUTCSeconds()",       ResultArray[UTC_SECONDS],DateCase.getUTCSeconds() );
  new TestCase( SECTION, DateString+".getUTCMilliseconds()",  ResultArray[UTC_MS],     DateCase.getUTCMilliseconds() );

  new TestCase( SECTION, DateString+".getFullYear()",         ResultArray[YEAR],       DateCase.getFullYear() );
  new TestCase( SECTION, DateString+".getMonth()",            ResultArray[MONTH],      DateCase.getMonth() );
  new TestCase( SECTION, DateString+".getDate()",             ResultArray[DATE],       DateCase.getDate() );
  new TestCase( SECTION, DateString+".getDay()",              ResultArray[DAY],        DateCase.getDay() );
  new TestCase( SECTION, DateString+".getHours()",            ResultArray[HOURS],      DateCase.getHours() );
  new TestCase( SECTION, DateString+".getMinutes()",          ResultArray[MINUTES],    DateCase.getMinutes() );
  new TestCase( SECTION, DateString+".getSeconds()",          ResultArray[SECONDS],    DateCase.getSeconds() );
  new TestCase( SECTION, DateString+".getMilliseconds()",     ResultArray[MS],         DateCase.getMilliseconds() );
}


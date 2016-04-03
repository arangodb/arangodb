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

gTestfile = '15.9.3.8-2.js';

/**
   File Name:          15.9.3.8.js
   ECMA Section:       15.9.3.8 The Date Constructor
   new Date( value )
   Description:        The [[Prototype]] property of the newly constructed
   object is set to the original Date prototype object,
   the one that is the initial valiue of Date.prototype.

   The [[Class]] property of the newly constructed object is
   set to "Date".

   The [[Value]] property of the newly constructed object is
   set as follows:

   1. Call ToPrimitive(value)
   2. If Type( Result(1) ) is String, then go to step 5.
   3. Let V be  ToNumber( Result(1) ).
   4. Set the [[Value]] property of the newly constructed
   object to TimeClip(V) and return.
   5. Parse Result(1) as a date, in exactly the same manner
   as for the parse method.  Let V be the time value for
   this date.
   6. Go to step 4.

   Author:             christine@netscape.com
   Date:               28 october 1997
   Version:            9706

*/

var VERSION = "ECMA_1";
startTest();
var SECTION = "15.9.3.8";
var TYPEOF  = "object";

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


//  for TCMS, the gTestcases array must be global.
var gTc= 0;
var TITLE = "Date constructor:  new Date( value )";
var SECTION = "15.9.3.8";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION +" " + TITLE );

// all the "ResultArrays" below are hard-coded to Pacific Standard Time values -
var TZ_ADJUST =  -TZ_PST * msPerHour;

addNewTestCase( new Date((new Date(0)).toUTCString()),
		"new Date(\""+ (new Date(0)).toUTCString()+"\" )",
		[0,1970,0,1,4,0,0,0,0,1969,11,31,3,16,0,0,0] );

addNewTestCase( new Date((new Date(1)).toString()),
		"new Date(\""+ (new Date(1)).toString()+"\" )",
		[0,1970,0,1,4,0,0,0,0,1969,11,31,3,16,0,0,0] );

addNewTestCase( new Date( TZ_ADJUST ),
		"new Date(" + TZ_ADJUST+")",
		[TZ_ADJUST,1970,0,1,4,8,0,0,0,1970,0,1,4,0,0,0,0] );

addNewTestCase( new Date((new Date(TZ_ADJUST)).toString()),
		"new Date(\""+ (new Date(TZ_ADJUST)).toString()+"\")",
		[TZ_ADJUST,1970,0,1,4,8,0,0,0,1970,0,1,4,0,0,0,0] );


addNewTestCase( new Date( (new Date(TZ_ADJUST)).toUTCString() ),
		"new Date(\""+ (new Date(TZ_ADJUST)).toUTCString()+"\")",
		[TZ_ADJUST,1970,0,1,4,8,0,0,0,1970,0,1,4,0,0,0,0] );

test();

function addNewTestCase( DateCase, DateString, ResultArray ) {
  //adjust hard-coded ResultArray for tester's timezone instead of PST
  adjustResultArray(ResultArray, 'msMode');

  new TestCase( SECTION, DateString+".getTime()", ResultArray[TIME],       DateCase.getTime() );
  new TestCase( SECTION, DateString+".valueOf()", ResultArray[TIME],       DateCase.valueOf() );
  new TestCase( SECTION, DateString+".getUTCFullYear()",      ResultArray[UTC_YEAR], DateCase.getUTCFullYear() );
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

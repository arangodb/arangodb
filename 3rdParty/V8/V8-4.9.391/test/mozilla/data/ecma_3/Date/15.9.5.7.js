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

/**
   File Name:    15.9.5.7.js
   ECMA Section: 15.9.5.7 Date.prototype.toLocaleTimeString()
   Description:
   This function returns a string value. The contents of the string are
   implementation dependent, but are intended to represent the "time"
   portion of the Date in the current time zone in a convenient,
   human-readable form.   We test the content of the string by checking
   that

   new Date(d.toDateString() + " " + d.toLocaleTimeString()) ==  d

   Author:  pschwartau@netscape.com                            
   Date:    14 november 2000
   Revised: 07 january 2002  because of a change in JS Date format:
   Revised: 21 November 2005 since the string comparison stuff is horked.
   bclary

   See http://bugzilla.mozilla.org/show_bug.cgi?id=118266 (SpiderMonkey)
   See http://bugzilla.mozilla.org/show_bug.cgi?id=118636 (Rhino)
*/
//-----------------------------------------------------------------------------
var gTestfile = '15.9.5.7.js';
var SECTION = "15.9.5.7";
var VERSION = "ECMA_3"; 
var TITLE   = "Date.prototype.toLocaleTimeString()";
  
var status = '';
var actual = ''; 
var expect = '';
var givenDate;
var year = '';
var regexp = '';
var TimeString = '';
var reducedDateString = '';
var hopeThisIsLocaleTimeString = '';
var cnERR ='OOPS! FATAL ERROR: no regexp match in extractLocaleTimeString()';

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

// first, a couple generic tests -

status = "typeof (now.toLocaleTimeString())"; 
actual =   typeof (now.toLocaleTimeString());
expect = "string";
addTestCase();

status = "Date.prototype.toLocaleTimeString.length";  
actual =  Date.prototype.toLocaleTimeString.length;
expect =  0;  
addTestCase();

// 1970
addDateTestCase(0);
addDateTestCase(TZ_ADJUST);
  
// 1900
addDateTestCase(TIME_1900);
addDateTestCase(TIME_1900 - TZ_ADJUST);

// 2000
addDateTestCase(TIME_2000);
addDateTestCase(TIME_2000 - TZ_ADJUST);
   
// 29 Feb 2000
addDateTestCase(UTC_29_FEB_2000);
addDateTestCase(UTC_29_FEB_2000 - 1000);
addDateTestCase(UTC_29_FEB_2000 - TZ_ADJUST);

// Now
addDateTestCase( TIME_NOW);
addDateTestCase( TIME_NOW - TZ_ADJUST);

// 2005
addDateTestCase(UTC_1_JAN_2005);
addDateTestCase(UTC_1_JAN_2005 - 1000);
addDateTestCase(UTC_1_JAN_2005 - TZ_ADJUST);

test();

function addTestCase()
{
  new TestCase(
    SECTION,
    status,
    expect,
    actual);
}


function addDateTestCase(date_given_in_milliseconds)
{
  var s = 'new Date(' +  date_given_in_milliseconds + ')';
  givenDate = new Date(date_given_in_milliseconds);
  
  status = 'd = ' + s +
    '; d == new Date(d.toDateString() + " " + d.toLocaleTimeString())';  
  expect = givenDate.toString();
  actual = new Date(givenDate.toDateString() +
                    ' ' + givenDate.toLocaleTimeString()).toString();
  addTestCase();
}


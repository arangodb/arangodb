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

gTestfile = '15.9.5.2.js';

/**
   File Name:          15.9.5.2.js
   ECMA Section:       15.9.5.2 Date.prototype.toString
   Description:
   This function returns a string value. The contents of the string are
   implementation dependent, but are intended to represent the Date in a
   convenient, human-readable form in the current time zone.

   The toString function is not generic; it generates a runtime error if its
   this value is not a Date object. Therefore it cannot be transferred to
   other kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.toString";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Date.prototype.toString.length",
	      0,
	      Date.prototype.toString.length );

var now = new Date();

// can't test the content of the string, but can verify that the string is
// parsable by Date.parse

new TestCase( SECTION,
	      "Math.abs(Date.parse(now.toString()) - now.valueOf()) < 1000",
	      true,
	      Math.abs(Date.parse(now.toString()) - now.valueOf()) < 1000 );

new TestCase( SECTION,
	      "typeof now.toString()",
	      "string",
	      typeof now.toString() );
// 1970

new TestCase( SECTION,
	      "Date.parse( (new Date(0)).toString() )",
	      0,
	      Date.parse( (new Date(0)).toString() ) );

new TestCase( SECTION,
	      "Date.parse( (new Date("+TZ_ADJUST+")).toString() )",
	      TZ_ADJUST,
	      Date.parse( (new Date(TZ_ADJUST)).toString() ) );

// 1900
new TestCase( SECTION,
	      "Date.parse( (new Date("+TIME_1900+")).toString() )",
	      TIME_1900,
	      Date.parse( (new Date(TIME_1900)).toString() ) );

new TestCase( SECTION,
	      "Date.parse( (new Date("+TIME_1900 -TZ_ADJUST+")).toString() )",
	      TIME_1900 -TZ_ADJUST,
	      Date.parse( (new Date(TIME_1900 -TZ_ADJUST)).toString() ) );

// 2000
new TestCase( SECTION,
	      "Date.parse( (new Date("+TIME_2000+")).toString() )",
	      TIME_2000,
	      Date.parse( (new Date(TIME_2000)).toString() ) );

new TestCase( SECTION,
	      "Date.parse( (new Date("+TIME_2000 -TZ_ADJUST+")).toString() )",
	      TIME_2000 -TZ_ADJUST,
	      Date.parse( (new Date(TIME_2000 -TZ_ADJUST)).toString() ) );

// 29 Feb 2000

new TestCase( SECTION,
	      "Date.parse( (new Date("+UTC_FEB_29_2000+")).toString() )",
	      UTC_FEB_29_2000,
	      Date.parse( (new Date(UTC_FEB_29_2000)).toString() ) );

new TestCase( SECTION,
	      "Date.parse( (new Date("+(UTC_FEB_29_2000-1000)+")).toString() )",
	      UTC_FEB_29_2000-1000,
	      Date.parse( (new Date(UTC_FEB_29_2000-1000)).toString() ) );


new TestCase( SECTION,
	      "Date.parse( (new Date("+(UTC_FEB_29_2000-TZ_ADJUST)+")).toString() )",
	      UTC_FEB_29_2000-TZ_ADJUST,
	      Date.parse( (new Date(UTC_FEB_29_2000-TZ_ADJUST)).toString() ) );
// 2O05

new TestCase( SECTION,
	      "Date.parse( (new Date("+UTC_JAN_1_2005+")).toString() )",
	      UTC_JAN_1_2005,
	      Date.parse( (new Date(UTC_JAN_1_2005)).toString() ) );

new TestCase( SECTION,
	      "Date.parse( (new Date("+(UTC_JAN_1_2005-1000)+")).toString() )",
	      UTC_JAN_1_2005-1000,
	      Date.parse( (new Date(UTC_JAN_1_2005-1000)).toString() ) );

new TestCase( SECTION,
	      "Date.parse( (new Date("+(UTC_JAN_1_2005-TZ_ADJUST)+")).toString() )",
	      UTC_JAN_1_2005-TZ_ADJUST,
	      Date.parse( (new Date(UTC_JAN_1_2005-TZ_ADJUST)).toString() ) );

test();

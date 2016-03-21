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

gTestfile = '15.9.5.5.js';

/**
   File Name:          15.9.5.5.js
   ECMA Section:       15.9.5.5
   Description:        Date.prototype.getYear

   This function is specified here for backwards compatibility only. The
   function getFullYear is much to be preferred for nearly all purposes,
   because it avoids the "year 2000 problem."

   1.  Let t be this time value.
   2.  If t is NaN, return NaN.
   3.  Return YearFromTime(LocalTime(t)) 1900.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.5";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.getYear()";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( TIME_NOW );
addTestCase( TIME_0000 );
addTestCase( TIME_1970 );
addTestCase( TIME_1900 );
addTestCase( TIME_2000 );
addTestCase( UTC_FEB_29_2000 );

new TestCase( SECTION,
	      "(new Date(NaN)).getYear()",
	      NaN,
	      (new Date(NaN)).getYear() );

new TestCase( SECTION,
	      "Date.prototype.getYear.length",
	      0,
	      Date.prototype.getYear.length );

test();

function addTestCase( t ) {
  new TestCase( SECTION,
		"(new Date("+t+")).getYear()",
		GetYear(YearFromTime(LocalTime(t))),
		(new Date(t)).getYear() );

  new TestCase( SECTION,
		"(new Date("+(t+1)+")).getYear()",
		GetYear(YearFromTime(LocalTime(t+1))),
		(new Date(t+1)).getYear() );

  new TestCase( SECTION,
		"(new Date("+(t-1)+")).getYear()",
		GetYear(YearFromTime(LocalTime(t-1))),
		(new Date(t-1)).getYear() );

  new TestCase( SECTION,
		"(new Date("+(t-TZ_ADJUST)+")).getYear()",
		GetYear(YearFromTime(LocalTime(t-TZ_ADJUST))),
		(new Date(t-TZ_ADJUST)).getYear() );

  new TestCase( SECTION,
		"(new Date("+(t+TZ_ADJUST)+")).getYear()",
		GetYear(YearFromTime(LocalTime(t+TZ_ADJUST))),
		(new Date(t+TZ_ADJUST)).getYear() );
}
function GetYear( year ) {
  return year - 1900;
}

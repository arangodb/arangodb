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

gTestfile = '15.9.5.23-4.js';

/**
   File Name:          15.9.5.23-2.js
   ECMA Section:       15.9.5.23
   Description:        Date.prototype.setTime

   1.  If the this value is not a Date object, generate a runtime error.
   2.  Call ToNumber(time).
   3.  Call TimeClip(Result(1)).
   4.  Set the [[Value]] property of the this value to Result(2).
   5.  Return the value of the [[Value]] property of the this value.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.23-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.setTime()";

writeHeaderToLog( SECTION + " "+ TITLE);

test_times = new Array( TIME_NOW, TIME_0000, TIME_1970, TIME_1900, TIME_2000,
			UTC_FEB_29_2000, UTC_JAN_1_2005 );


for ( var j = 0; j < test_times.length; j++ ) {
  addTestCase( new Date(TIME_0000), test_times[j] );
}

new TestCase( SECTION,
	      "(new Date(NaN)).setTime()",
	      NaN,
	      (new Date(NaN)).setTime() );

new TestCase( SECTION,
	      "Date.prototype.setTime.length",
	      1,
	      Date.prototype.setTime.length );
test();

function addTestCase( d, t ) {
  new TestCase( SECTION,
		"( "+d+" ).setTime("+t+")",
		t,
		d.setTime(t) );

  new TestCase( SECTION,
		"( "+d+" ).setTime("+(t+1.1)+")",
		TimeClip(t+1.1),
		d.setTime(t+1.1) );

  new TestCase( SECTION,
		"( "+d+" ).setTime("+(t+1)+")",
		t+1,
		d.setTime(t+1) );

  new TestCase( SECTION,
		"( "+d+" ).setTime("+(t-1)+")",
		t-1,
		d.setTime(t-1) );

  new TestCase( SECTION,
		"( "+d+" ).setTime("+(t-TZ_ADJUST)+")",
		t-TZ_ADJUST,
		d.setTime(t-TZ_ADJUST) );

  new TestCase( SECTION,
		"( "+d+" ).setTime("+(t+TZ_ADJUST)+")",
		t+TZ_ADJUST,
		d.setTime(t+TZ_ADJUST) );
}

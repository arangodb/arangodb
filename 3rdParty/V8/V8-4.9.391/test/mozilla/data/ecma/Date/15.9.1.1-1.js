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

gTestfile = '15.9.1.1-1.js';

/**
   File Name:          15.9.1.1-1.js
   ECMA Section:       15.9.1.1 Time Range
   Description:
   - leap seconds are ignored
   - assume 86400000 ms / day
   - numbers range fom +/- 9,007,199,254,740,991
   - ms precision for any instant that is within
   approximately +/-285,616 years from 1 jan 1970
   UTC
   - range of times supported is -100,000,000 days
   to 100,000,000 days from 1 jan 1970 12:00 am
   - time supported is 8.64e5*10e8 milliseconds from
   1 jan 1970 UTC  (+/-273972.6027397 years)

   -   this test generates its own data -- it does not
   read data from a file.
   Author:             christine@netscape.com
   Date:               7 july 1997

   Static variables:
   FOUR_HUNDRED_YEARS

*/

//  every one hundred years contains:
//    24 years with 366 days
//
//  every four hundred years contains:
//    97 years with 366 days
//   303 years with 365 days
//
//   86400000*365*97    =    3067372800000
//  +86400000*366*303   =  + 9555408000000
//                      =    1.26227808e+13
var FOUR_HUNDRED_YEARS = 1.26227808e+13;
var SECTION         =  "15.9.1.1-1";

writeHeaderToLog("15.9.1.1 Time Range");

var M_SECS;
var CURRENT_YEAR;

for ( M_SECS = 0, CURRENT_YEAR = 1970;
      M_SECS < 8640000000000000;
      M_SECS += FOUR_HUNDRED_YEARS, CURRENT_YEAR += 400 ) {

  new TestCase( SECTION,  
		"new Date("+M_SECS+")",
		CURRENT_YEAR,
		(new Date( M_SECS)).getUTCFullYear() );
}

test();


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

gTestfile = '15.9.2.1.js';

/**
   File Name:          15.9.2.1.js
   ECMA Section:       15.9.2.1 Date constructor used as a function
   Date( year, month, date, hours, minutes, seconds, ms )
   Description:        The arguments are accepted, but are completely ignored.
   A string is created and returned as if by the
   expression (new Date()).toString().

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var VERSION =   "ECMA_1";
startTest();
var SECTION =   "15.9.2.1";
var TITLE =     "Date Constructor used as a function";
var TYPEOF  =   "string";
var TOLERANCE = 1000;

writeHeaderToLog("15.9.2.1 The Date Constructor Called as a Function:  " +
		 "Date( year, month, date, hours, minutes, seconds, ms )" );

// allow up to 1 second difference due to possibility
// the date may change by 1 second in between calls to Date

var d1;
var d2;

// Dates around 1970

d1 = new Date();
d2 = Date.parse(Date(1970,0,1,0,0,0,0));
new TestCase(SECTION, "Date(1970,0,1,0,0,0,0)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(1969,11,31,15,59,59,999));
new TestCase(SECTION, "Date(1969,11,31,15,59,59,999)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(1969,11,31,16,0,0,0));
new TestCase(SECTION, "Date(1969,11,31,16,0,0,0)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(1969,11,31,16,0,0,1));
new TestCase(SECTION, "Date(1969,11,31,16,0,0,1)", true, d2 - d1 <= 1000);

// Dates around 2000
d1 = new Date();
d2 = Date.parse(Date(1999,11,15,59,59,999));
new TestCase(SECTION, "Date(1999,11,15,59,59,999)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(1999,11,16,0,0,0,0));
new TestCase(SECTION, "Date(1999,11,16,0,0,0,0)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(1999,11,31,23,59,59,999));
new TestCase(SECTION, "Date(1999,11,31,23,59,59,999)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2000,0,0,0,0,0,0));
new TestCase(SECTION, "Date(2000,0,1,0,0,0,0)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2000,0,0,0,0,0,1));
new TestCase(SECTION, "Date(2000,0,1,0,0,0,1)", true, d2 - d1 <= 1000);

// Dates around 1900

d1 = new Date();
d2 = Date.parse(Date(1899,11,31,23,59,59,999));
new TestCase(SECTION, "Date(1899,11,31,23,59,59,999)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(1900,0,1,0,0,0,0));
new TestCase(SECTION, "Date(1900,0,1,0,0,0,0)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(1900,0,1,0,0,0,1));
new TestCase(SECTION, "Date(1900,0,1,0,0,0,1)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(1899,11,31,16,0,0,0,0));
new TestCase(SECTION, "Date(1899,11,31,16,0,0,0,0)", true, d2 - d1 <= 1000);

// Dates around feb 29, 2000

d1 = new Date();
d2 = Date.parse(Date(2000,1,29,0,0,0,0));
new TestCase(SECTION, "Date(2000,1,29,0,0,0,0)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2000,1,28,23,59,59,999));
new TestCase(SECTION, "Date(2000,1,28,23,59,59,999)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2000,1,27,16,0,0,0));
new TestCase(SECTION, "Date(2000,1,27,16,0,0,0)", true, d2 - d1 <= 1000);

// Dates around jan 1, 2005
d1 = new Date();
d2 = Date.parse(Date(2004,11,31,23,59,59,999));
new TestCase(SECTION, "Date(2004,11,31,23,59,59,999)",  true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2005,0,1,0,0,0,0));
new TestCase(SECTION, "Date(2005,0,1,0,0,0,0)",  true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2005,0,1,0,0,0,1));
new TestCase(SECTION, "Date(2005,0,1,0,0,0,1)",  true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2004,11,31,16,0,0,0,0));
new TestCase(SECTION, "Date(2004,11,31,16,0,0,0,0)",  true, d2 - d1 <= 1000);

// Dates around jan 1, 2032
d1 = new Date();
d2 = Date.parse(Date(2031,11,31,23,59,59,999));
new TestCase(SECTION, "Date(2031,11,31,23,59,59,999)",  true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2032,0,1,0,0,0,0));
new TestCase(SECTION, "Date(2032,0,1,0,0,0,0)",  true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2032,0,1,0,0,0,1));
new TestCase(SECTION, "Date(2032,0,1,0,0,0,1)",  true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2031,11,31,16,0,0,0,0));
new TestCase(SECTION, "Date(2031,11,31,16,0,0,0,0)",  true, d2 - d1 <= 1000);

test();

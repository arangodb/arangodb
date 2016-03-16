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

gTestfile = '15.9.2.2-6.js';

/**
   File Name:          15.9.2.2.js
   ECMA Section:       15.9.2.2 Date constructor used as a function
   Date( year, month, date, hours, minutes, seconds )
   Description:        The arguments are accepted, but are completely ignored.
   A string is created and returned as if by the
   expression (new Date()).toString().

   Author:             christine@netscape.com
   Date:               28 october 1997
   Version:            9706

*/
var VERSION = 9706;
startTest();
var SECTION = "15.9.2.2";
var TOLERANCE = 100;
var TITLE = "The Date Constructor Called as a Function";

writeHeaderToLog(SECTION+" "+TITLE );

// allow up to 1 second difference due to possibility
// the date may change by 1 second in between calls to Date

var d1;
var d2;

// Dates around jan 1, 2032
d1 = new Date();
d2 = Date.parse(Date(2031,11,31,23,59,59));
new TestCase(SECTION, "Date(2031,11,31,23,59,59)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2032,0,1,0,0,0));
new TestCase(SECTION, "Date(2032,0,1,0,0,0)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2032,0,1,0,0,1));
new TestCase(SECTION, "Date(2032,0,1,0,0,1)", true, d2 - d1 <= 1000);

d1 = new Date();
d2 = Date.parse(Date(2031,11,31,16,0,0,0));
new TestCase(SECTION, "Date(2031,11,31,16,0,0,0)", true, d2 - d1 <= 1000);

test();

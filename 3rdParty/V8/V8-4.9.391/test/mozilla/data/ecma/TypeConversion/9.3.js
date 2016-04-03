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

gTestfile = '9.3.js';

/**
   File Name:          9.3.js
   ECMA Section:       9.3  Type Conversion:  ToNumber
   Description:        rules for converting an argument to a number.
   see 9.3.1 for cases for converting strings to numbers.
   special cases:
   undefined           NaN
   Null                NaN
   Boolean             1 if true; +0 if false
   Number              the argument ( no conversion )
   String              see test 9.3.1
   Object              see test 9.3-1

   For ToNumber applied to the String type, see test 9.3.1.
   For ToNumber applied to the object type, see test 9.3-1.

   Author:             christine@netscape.com
   Date:               10 july 1997

*/
var SECTION = "9.3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "ToNumber";

writeHeaderToLog( SECTION + " "+ TITLE);

// special cases here

new TestCase( SECTION,   "Number()",                      0,              Number() );
new TestCase( SECTION,   "Number(eval('var x'))",         Number.NaN,     Number(eval("var x")) );
new TestCase( SECTION,   "Number(void 0)",                Number.NaN,     Number(void 0) );
new TestCase( SECTION,   "Number(null)",                  0,              Number(null) );
new TestCase( SECTION,   "Number(true)",                  1,              Number(true) );
new TestCase( SECTION,   "Number(false)",                 0,              Number(false) );
new TestCase( SECTION,   "Number(0)",                     0,              Number(0) );
new TestCase( SECTION,   "Number(-0)",                    -0,             Number(-0) );
new TestCase( SECTION,   "Number(1)",                     1,              Number(1) );
new TestCase( SECTION,   "Number(-1)",                    -1,             Number(-1) );
new TestCase( SECTION,   "Number(Number.MAX_VALUE)",      1.7976931348623157e308, Number(Number.MAX_VALUE) );
new TestCase( SECTION,   "Number(Number.MIN_VALUE)",      5e-324,         Number(Number.MIN_VALUE) );

new TestCase( SECTION,   "Number(Number.NaN)",                Number.NaN,                 Number(Number.NaN) );
new TestCase( SECTION,   "Number(Number.POSITIVE_INFINITY)",  Number.POSITIVE_INFINITY,   Number(Number.POSITIVE_INFINITY) );
new TestCase( SECTION,   "Number(Number.NEGATIVE_INFINITY)",  Number.NEGATIVE_INFINITY,   Number(Number.NEGATIVE_INFINITY) );

test();

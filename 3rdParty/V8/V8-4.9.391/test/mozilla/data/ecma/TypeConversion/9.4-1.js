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

gTestfile = '9.4-1.js';

/**
   File Name:          9.4-1.js
   ECMA Section:       9.4 ToInteger
   Description:        1.  Call ToNumber on the input argument
   2.  If Result(1) is NaN, return +0
   3.  If Result(1) is +0, -0, Infinity, or -Infinity,
   return Result(1).
   4.  Compute sign(Result(1)) * floor(abs(Result(1))).
   5.  Return Result(4).

   To test ToInteger, this test uses new Date(value),
   15.9.3.7.  The Date constructor sets the [[Value]]
   property of the new object to TimeClip(value), which
   uses the rules:

   TimeClip(time)
   1. If time is not finite, return NaN
   2. If abs(Result(1)) > 8.64e15, return NaN
   3. Return an implementation dependent choice of either
   ToInteger(Result(2)) or ToInteger(Result(2)) + (+0)
   (Adding a positive 0 converts -0 to +0).

   This tests ToInteger for values -8.64e15 > value > 8.64e15,
   not including -0 and +0.

   For additional special cases (0, +0, Infinity, -Infinity,
   and NaN, see 9.4-2.js).  For value is String, see 9.4-3.js.

   Author:             christine@netscape.com
   Date:               10 july 1997

*/
var SECTION = "9.4-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "ToInteger";

writeHeaderToLog( SECTION + " "+ TITLE);

// some special cases

new TestCase( SECTION,  "td = new Date(Number.NaN); td.valueOf()",  Number.NaN, eval("td = new Date(Number.NaN); td.valueOf()") );
new TestCase( SECTION,  "td = new Date(Infinity); td.valueOf()",    Number.NaN, eval("td = new Date(Number.POSITIVE_INFINITY); td.valueOf()") );
new TestCase( SECTION,  "td = new Date(-Infinity); td.valueOf()",   Number.NaN, eval("td = new Date(Number.NEGATIVE_INFINITY); td.valueOf()") );
new TestCase( SECTION,  "td = new Date(-0); td.valueOf()",          -0,         eval("td = new Date(-0); td.valueOf()" ) );
new TestCase( SECTION,  "td = new Date(0); td.valueOf()",           0,          eval("td = new Date(0); td.valueOf()") );

// value is not an integer

new TestCase( SECTION,  "td = new Date(3.14159); td.valueOf()",     3,          eval("td = new Date(3.14159); td.valueOf()") );
new TestCase( SECTION,  "td = new Date(Math.PI); td.valueOf()",     3,          eval("td = new Date(Math.PI); td.valueOf()") );
new TestCase( SECTION,  "td = new Date(-Math.PI);td.valueOf()",     -3,         eval("td = new Date(-Math.PI);td.valueOf()") );
new TestCase( SECTION,  "td = new Date(3.14159e2); td.valueOf()",   314,        eval("td = new Date(3.14159e2); td.valueOf()") );

new TestCase( SECTION,  "td = new Date(.692147e1); td.valueOf()",   6,          eval("td = new Date(.692147e1);td.valueOf()") );
new TestCase( SECTION,  "td = new Date(-.692147e1);td.valueOf()",   -6,         eval("td = new Date(-.692147e1);td.valueOf()") );

// value is not a number

new TestCase( SECTION,  "td = new Date(true); td.valueOf()",        1,          eval("td = new Date(true); td.valueOf()" ) );
new TestCase( SECTION,  "td = new Date(false); td.valueOf()",       0,          eval("td = new Date(false); td.valueOf()") );

new TestCase( SECTION,  "td = new Date(new Number(Math.PI)); td.valueOf()",  3, eval("td = new Date(new Number(Math.PI)); td.valueOf()") );
new TestCase( SECTION,  "td = new Date(new Number(Math.PI)); td.valueOf()",  3, eval("td = new Date(new Number(Math.PI)); td.valueOf()") );

// edge cases
new TestCase( SECTION,  "td = new Date(8.64e15); td.valueOf()",     8.64e15,    eval("td = new Date(8.64e15); td.valueOf()") );
new TestCase( SECTION,  "td = new Date(-8.64e15); td.valueOf()",    -8.64e15,   eval("td = new Date(-8.64e15); td.valueOf()") );
new TestCase( SECTION,  "td = new Date(8.64e-15); td.valueOf()",    0,          eval("td = new Date(8.64e-15); td.valueOf()") );
new TestCase( SECTION,  "td = new Date(-8.64e-15); td.valueOf()",   0,          eval("td = new Date(-8.64e-15); td.valueOf()") );

test();

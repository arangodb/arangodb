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

gTestfile = '15.8.2.17.js';

/**
   File Name:          15.8.2.17.js
   ECMA Section:       15.8.2.17  Math.sqrt(x)
   Description:        return an approximation to the squareroot of the argument.
   special cases:
   -   if x is NaN         return NaN
   -   if x < 0            return NaN
   -   if x == 0           return 0
   -   if x == -0          return -0
   -   if x == Infinity    return Infinity
   Author:             christine@netscape.com
   Date:               7 july 1997
*/

var SECTION = "15.8.2.17";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Math.sqrt(x)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.sqrt.length",
	      1,
	      Math.sqrt.length );

new TestCase( SECTION,
	      "Math.sqrt()",
	      Number.NaN,
	      Math.sqrt() );

new TestCase( SECTION,
	      "Math.sqrt(void 0)",
	      Number.NaN,
	      Math.sqrt(void 0) );

new TestCase( SECTION,
	      "Math.sqrt(null)",
	      0,
	      Math.sqrt(null) );

new TestCase( SECTION,
	      "Math.sqrt(true)",
	      1,
	      Math.sqrt(1) );

new TestCase( SECTION,
	      "Math.sqrt(false)",
	      0,
	      Math.sqrt(false) );

new TestCase( SECTION,
	      "Math.sqrt('225')",
	      15,
	      Math.sqrt('225') );

new TestCase( SECTION,
	      "Math.sqrt(NaN)",
	      Number.NaN,
	      Math.sqrt(Number.NaN) );

new TestCase( SECTION,
	      "Math.sqrt(-Infinity)",
	      Number.NaN,
	      Math.sqrt(Number.NEGATIVE_INFINITY));

new TestCase( SECTION,
	      "Math.sqrt(-1)",
	      Number.NaN,
	      Math.sqrt(-1));

new TestCase( SECTION,
	      "Math.sqrt(-0.5)",
	      Number.NaN,
	      Math.sqrt(-0.5));

new TestCase( SECTION,
	      "Math.sqrt(0)",
	      0,
	      Math.sqrt(0));

new TestCase( SECTION,
	      "Math.sqrt(-0)",
	      -0,
	      Math.sqrt(-0));

new TestCase( SECTION,
	      "Infinity/Math.sqrt(-0)",
	      -Infinity,
	      Infinity/Math.sqrt(-0) );

new TestCase( SECTION,
	      "Math.sqrt(Infinity)",
	      Number.POSITIVE_INFINITY,
	      Math.sqrt(Number.POSITIVE_INFINITY));

new TestCase( SECTION,
	      "Math.sqrt(1)",
	      1,
	      Math.sqrt(1));

new TestCase( SECTION,
	      "Math.sqrt(2)",
	      Math.SQRT2,
	      Math.sqrt(2));

new TestCase( SECTION,
	      "Math.sqrt(0.5)",
	      Math.SQRT1_2,
	      Math.sqrt(0.5));

new TestCase( SECTION,
	      "Math.sqrt(4)",
	      2,
	      Math.sqrt(4));

new TestCase( SECTION,
	      "Math.sqrt(9)",
	      3,
	      Math.sqrt(9));

new TestCase( SECTION,
	      "Math.sqrt(16)",
	      4,
	      Math.sqrt(16));

new TestCase( SECTION,
	      "Math.sqrt(25)",
	      5,
	      Math.sqrt(25));

new TestCase( SECTION,
	      "Math.sqrt(36)",
	      6,
	      Math.sqrt(36));

new TestCase( SECTION,
	      "Math.sqrt(49)",
	      7,
	      Math.sqrt(49));

new TestCase( SECTION,
	      "Math.sqrt(64)",
	      8,
	      Math.sqrt(64));

new TestCase( SECTION,
	      "Math.sqrt(256)",
	      16,
	      Math.sqrt(256));

new TestCase( SECTION,
	      "Math.sqrt(10000)",
	      100,
	      Math.sqrt(10000));

new TestCase( SECTION,
	      "Math.sqrt(65536)",
	      256,
	      Math.sqrt(65536));

new TestCase( SECTION,
	      "Math.sqrt(0.09)",
	      0.3,
	      Math.sqrt(0.09));

new TestCase( SECTION,
	      "Math.sqrt(0.01)",
	      0.1,
	      Math.sqrt(0.01));

new TestCase( SECTION,
	      "Math.sqrt(0.00000001)",
	      0.0001,
	      Math.sqrt(0.00000001));

test();

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

gTestfile = '15.8.2.2.js';

/**
   File Name:          15.8.2.2.js
   ECMA Section:       15.8.2.2 acos( x )
   Description:        return an approximation to the arc cosine of the
   argument.  the result is expressed in radians and
   range is from +0 to +PI.  special cases:
   - if x is NaN, return NaN
   - if x > 1, the result is NaN
   - if x < -1, the result is NaN
   - if x == 1, the result is +0
   Author:             christine@netscape.com
   Date:               7 july 1997
*/
var SECTION = "15.8.2.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Math.acos()";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.acos.length",
	      1,
	      Math.acos.length );

new TestCase( SECTION,
	      "Math.acos(void 0)",
	      Number.NaN,
	      Math.acos(void 0) );

new TestCase( SECTION,
	      "Math.acos()",
	      Number.NaN,
	      Math.acos() );

new TestCase( SECTION,
	      "Math.acos(null)",
	      Math.PI/2,
	      Math.acos(null) );

new TestCase( SECTION,
	      "Math.acos(NaN)",
	      Number.NaN,
	      Math.acos(Number.NaN) );

new TestCase( SECTION,
	      "Math.acos(a string)",
	      Number.NaN,
	      Math.acos("a string") );

new TestCase( SECTION,
	      "Math.acos('0')",
	      Math.PI/2,
	      Math.acos('0') );

new TestCase( SECTION,
	      "Math.acos('1')",
	      0,
	      Math.acos('1') );

new TestCase( SECTION,
	      "Math.acos('-1')",
	      Math.PI,
	      Math.acos('-1') );

new TestCase( SECTION,
	      "Math.acos(1.00000001)",
	      Number.NaN,
	      Math.acos(1.00000001) );

new TestCase( SECTION,
	      "Math.acos(11.00000001)",
	      Number.NaN,
	      Math.acos(-1.00000001) );

new TestCase( SECTION,
	      "Math.acos(1)",
	      0,
	      Math.acos(1)          );

new TestCase( SECTION,
	      "Math.acos(-1)",
	      Math.PI,
	      Math.acos(-1)         );

new TestCase( SECTION,
	      "Math.acos(0)",
	      Math.PI/2,
	      Math.acos(0)          );

new TestCase( SECTION,
	      "Math.acos(-0)",
	      Math.PI/2,
	      Math.acos(-0)         );

new TestCase( SECTION,
	      "Math.acos(Math.SQRT1_2)",
	      Math.PI/4,
	      Math.acos(Math.SQRT1_2));

new TestCase( SECTION,
	      "Math.acos(-Math.SQRT1_2)",
	      Math.PI/4*3,
	      Math.acos(-Math.SQRT1_2));

new TestCase( SECTION,
	      "Math.acos(0.9999619230642)",
	      Math.PI/360,
	      Math.acos(0.9999619230642));

new TestCase( SECTION,
	      "Math.acos(-3.0)",
	      Number.NaN,
	      Math.acos(-3.0));

test();

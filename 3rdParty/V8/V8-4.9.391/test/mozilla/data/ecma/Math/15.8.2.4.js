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

gTestfile = '15.8.2.4.js';

/**
   File Name:          15.8.2.4.js
   ECMA Section:       15.8.2.4 atan( x )
   Description:        return an approximation to the arc tangent of the
   argument.  the result is expressed in radians and
   range is from -PI/2 to +PI/2.  special cases:
   - if x is NaN,  the result is NaN
   - if x == +0,   the result is +0
   - if x == -0,   the result is -0
   - if x == +Infinity,    the result is approximately +PI/2
   - if x == -Infinity,    the result is approximately -PI/2
   Author:             christine@netscape.com
   Date:               7 july 1997

*/

var SECTION = "15.8.2.4";
var VERSION = "ECMA_1";
var TITLE   = "Math.atan()";
var BUGNUMBER="77391";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.atan.length",
	      1,
	      Math.atan.length );

new TestCase( SECTION,
	      "Math.atan()",
	      Number.NaN,
	      Math.atan() );

new TestCase( SECTION,
	      "Math.atan(void 0)",
	      Number.NaN,
	      Math.atan(void 0) );

new TestCase( SECTION,
	      "Math.atan(null)",
	      0,
	      Math.atan(null) );

new TestCase( SECTION,
	      "Math.atan(NaN)",
	      Number.NaN,
	      Math.atan(Number.NaN) );

new TestCase( SECTION,
	      "Math.atan('a string')",
	      Number.NaN,
	      Math.atan("a string") );

new TestCase( SECTION,
	      "Math.atan('0')",
	      0,
	      Math.atan('0') );

new TestCase( SECTION,
	      "Math.atan('1')",
	      Math.PI/4,
	      Math.atan('1') );

new TestCase( SECTION,
	      "Math.atan('-1')",
	      -Math.PI/4,
	      Math.atan('-1') );

new TestCase( SECTION,
	      "Math.atan('Infinity)",
	      Math.PI/2,
	      Math.atan('Infinity') );

new TestCase( SECTION,
	      "Math.atan('-Infinity)",
	      -Math.PI/2,
	      Math.atan('-Infinity') );

new TestCase( SECTION,
	      "Math.atan(0)",
	      0,
	      Math.atan(0)          );

new TestCase( SECTION,
	      "Math.atan(-0)",	
	      -0,
	      Math.atan(-0)         );

new TestCase( SECTION,
	      "Infinity/Math.atan(-0)",
	      -Infinity,
	      Infinity/Math.atan(-0) );

new TestCase( SECTION,
	      "Math.atan(Infinity)",
	      Math.PI/2,
	      Math.atan(Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.atan(-Infinity)",
	      -Math.PI/2,
	      Math.atan(Number.NEGATIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.atan(1)",
	      Math.PI/4,
	      Math.atan(1)          );

new TestCase( SECTION,
	      "Math.atan(-1)",
	      -Math.PI/4,
	      Math.atan(-1)         );

test();

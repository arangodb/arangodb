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

gTestfile = '15.8.2.8.js';

/**
   File Name:          15.8.2.8.js
   ECMA Section:       15.8.2.8  Math.exp(x)
   Description:        return an approximation to the exponential function of
   the argument (e raised to the power of the argument)
   special cases:
   -   if x is NaN         return NaN
   -   if x is 0           return 1
   -   if x is -0          return 1
   -   if x is Infinity    return Infinity
   -   if x is -Infinity   return 0
   Author:             christine@netscape.com
   Date:               7 july 1997
*/


var SECTION = "15.8.2.8";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Math.exp(x)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.exp.length",
	      1,
	      Math.exp.length );

new TestCase( SECTION,
	      "Math.exp()",
	      Number.NaN,
	      Math.exp() );

new TestCase( SECTION,
	      "Math.exp(null)",
	      1,
	      Math.exp(null) );

new TestCase( SECTION,
	      "Math.exp(void 0)",
	      Number.NaN,
	      Math.exp(void 0) );

new TestCase( SECTION,
	      "Math.exp(1)",
	      Math.E,
	      Math.exp(1) );

new TestCase( SECTION,
	      "Math.exp(true)",
	      Math.E,
	      Math.exp(true) );

new TestCase( SECTION,
	      "Math.exp(false)",
	      1,
	      Math.exp(false) );

new TestCase( SECTION,
	      "Math.exp('1')",
	      Math.E,
	      Math.exp('1') );

new TestCase( SECTION,
	      "Math.exp('0')",
	      1,
	      Math.exp('0') );

new TestCase( SECTION,
	      "Math.exp(NaN)",
	      Number.NaN,
	      Math.exp(Number.NaN) );

new TestCase( SECTION,
	      "Math.exp(0)",
	      1,
	      Math.exp(0)          );

new TestCase( SECTION,
	      "Math.exp(-0)",
	      1,
	      Math.exp(-0)         );

new TestCase( SECTION,
	      "Math.exp(Infinity)",
	      Number.POSITIVE_INFINITY,
	      Math.exp(Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.exp(-Infinity)", 
	      0,
	      Math.exp(Number.NEGATIVE_INFINITY) );

test();

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

gTestfile = '15.8.2.11.js';

/**
   File Name:          15.8.2.11.js
   ECMA Section:       15.8.2.11 Math.max(x, y)
   Description:        return the smaller of the two arguments.
   special cases:
   - if x is NaN or y is NaN   return NaN
   - if x < y                  return x
   - if y > x                  return y
   - if x is +0 and y is +0    return +0
   - if x is +0 and y is -0    return -0
   - if x is -0 and y is +0    return -0
   - if x is -0 and y is -0    return -0
   Author:             christine@netscape.com
   Date:               7 july 1997
*/

var SECTION = "15.8.2.11";
var VERSION = "ECMA_1";
var TITLE   = "Math.max(x, y)";
var BUGNUMBER="76439";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.max.length",
	      2,
	      Math.max.length );

new TestCase( SECTION,
	      "Math.max()",
	      -Infinity,
	      Math.max() );

new TestCase( SECTION,
	      "Math.max(void 0, 1)",
	      Number.NaN,
	      Math.max( void 0, 1 ) );

new TestCase( SECTION,
	      "Math.max(void 0, void 0)",
	      Number.NaN,
	      Math.max( void 0, void 0 ) );

new TestCase( SECTION,
	      "Math.max(null, 1)",
	      1,
	      Math.max( null, 1 ) );

new TestCase( SECTION,
	      "Math.max(-1, null)",
	      0,
	      Math.max( -1, null ) );

new TestCase( SECTION,
	      "Math.max(true, false)",
	      1,
	      Math.max(true,false) );

new TestCase( SECTION,
	      "Math.max('-99','99')",
	      99,
	      Math.max( "-99","99") );

new TestCase( SECTION,
	      "Math.max(NaN, Infinity)",
	      Number.NaN,
	      Math.max(Number.NaN,Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.max(NaN, 0)",
	      Number.NaN,
	      Math.max(Number.NaN, 0) );

new TestCase( SECTION,
	      "Math.max('a string', 0)",
	      Number.NaN,
	      Math.max("a string", 0) );

new TestCase( SECTION,
	      "Math.max(NaN, 1)",
	      Number.NaN,
	      Math.max(Number.NaN,1) );

new TestCase( SECTION,
	      "Math.max('a string',Infinity)",
	      Number.NaN,
	      Math.max("a string", Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.max(Infinity, NaN)",
	      Number.NaN,
	      Math.max( Number.POSITIVE_INFINITY, Number.NaN) );

new TestCase( SECTION,
	      "Math.max(NaN, NaN)",
	      Number.NaN,
	      Math.max(Number.NaN, Number.NaN) );

new TestCase( SECTION,
	      "Math.max(0,NaN)",
	      Number.NaN,
	      Math.max(0,Number.NaN) );

new TestCase( SECTION,
	      "Math.max(1, NaN)",
	      Number.NaN,
	      Math.max(1, Number.NaN) );

new TestCase( SECTION,
	      "Math.max(0,0)",
              0,
	      Math.max(0,0) );

new TestCase( SECTION,
	      "Math.max(0,-0)",
	      0,
	      Math.max(0,-0) );

new TestCase( SECTION,
	      "Math.max(-0,0)",
	      0,
	      Math.max(-0,0) );

new TestCase( SECTION,
	      "Math.max(-0,-0)",
	      -0,
	      Math.max(-0,-0) );

new TestCase( SECTION,
	      "Infinity/Math.max(-0,-0)",
	      -Infinity,
	      Infinity/Math.max(-0,-0) );

new TestCase( SECTION,
	      "Math.max(Infinity, Number.MAX_VALUE)", Number.POSITIVE_INFINITY,
	      Math.max(Number.POSITIVE_INFINITY, Number.MAX_VALUE) );

new TestCase( SECTION,
	      "Math.max(Infinity, Infinity)",
	      Number.POSITIVE_INFINITY,
	      Math.max(Number.POSITIVE_INFINITY,Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.max(-Infinity,-Infinity)",
	      Number.NEGATIVE_INFINITY,
	      Math.max(Number.NEGATIVE_INFINITY,Number.NEGATIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.max(1,.99999999999999)",
	      1,
	      Math.max(1,.99999999999999) );

new TestCase( SECTION,
	      "Math.max(-1,-.99999999999999)",
	      -.99999999999999,
	      Math.max(-1,-.99999999999999) );

test();

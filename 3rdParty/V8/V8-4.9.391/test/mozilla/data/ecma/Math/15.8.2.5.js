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

gTestfile = '15.8.2.5.js';

/**
   File Name:          15.8.2.5.js
   ECMA Section:       15.8.2.5 atan2( y, x )
   Description:

   Author:             christine@netscape.com
   Date:               7 july 1997

*/
var SECTION = "15.8.2.5";
var VERSION = "ECMA_1";
var TITLE   = "Math.atan2(x,y)";
var BUGNUMBER="76111";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.atan2.length",
	      2,
	      Math.atan2.length );

new TestCase( SECTION,
	      "Math.atan2(NaN, 0)",
	      Number.NaN,
	      Math.atan2(Number.NaN,0) );

new TestCase( SECTION,
	      "Math.atan2(null, null)",
	      0,
	      Math.atan2(null, null) );

new TestCase( SECTION,
	      "Math.atan2(void 0, void 0)",
	      Number.NaN,
	      Math.atan2(void 0, void 0) );

new TestCase( SECTION,
	      "Math.atan2()",
	      Number.NaN,
	      Math.atan2() );

new TestCase( SECTION,
	      "Math.atan2(0, NaN)",
	      Number.NaN,
	      Math.atan2(0,Number.NaN) );

new TestCase( SECTION,
	      "Math.atan2(1, 0)",
	      Math.PI/2,
	      Math.atan2(1,0)          );

new TestCase( SECTION,
	      "Math.atan2(1,-0)",
	      Math.PI/2,
	      Math.atan2(1,-0)         );

new TestCase( SECTION,
	      "Math.atan2(0,0.001)",
	      0,
	      Math.atan2(0,0.001)      );

new TestCase( SECTION,
	      "Math.atan2(0,0)",
	      0,
	      Math.atan2(0,0)          );

new TestCase( SECTION,
	      "Math.atan2(0, -0)",
	      Math.PI,
	      Math.atan2(0,-0)         );

new TestCase( SECTION,
	      "Math.atan2(0, -1)",
	      Math.PI,
	      Math.atan2(0, -1)        );

new TestCase( SECTION,
	      "Math.atan2(-0, 1)",
	      -0,
	      Math.atan2(-0, 1)        );

new TestCase( SECTION,
	      "Infinity/Math.atan2(-0, 1)",
	      -Infinity,
	      Infinity/Math.atan2(-0,1) );

new TestCase( SECTION,
	      "Math.atan2(-0,	0)",
	      -0,
	      Math.atan2(-0,0)         );

new TestCase( SECTION,
	      "Math.atan2(-0,	-0)",
	      -Math.PI,
	      Math.atan2(-0, -0)       );

new TestCase( SECTION,
	      "Math.atan2(-0,	-1)",
	      -Math.PI,
	      Math.atan2(-0, -1)       );

new TestCase( SECTION,
	      "Math.atan2(-1,	0)",
	      -Math.PI/2,
	      Math.atan2(-1, 0)        );

new TestCase( SECTION,
	      "Math.atan2(-1,	-0)",
	      -Math.PI/2,
	      Math.atan2(-1, -0)       );

new TestCase( SECTION,
	      "Math.atan2(1, Infinity)",
	      0,
	      Math.atan2(1, Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.atan2(1,-Infinity)", 
	      Math.PI,
	      Math.atan2(1, Number.NEGATIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.atan2(-1, Infinity)",
	      -0,
	      Math.atan2(-1,Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Infinity/Math.atan2(-1, Infinity)",
	      -Infinity, 
	      Infinity/Math.atan2(-1,Infinity) );

new TestCase( SECTION,
	      "Math.atan2(-1,-Infinity)",
	      -Math.PI,
	      Math.atan2(-1,Number.NEGATIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.atan2(Infinity, 0)", 
	      Math.PI/2,
	      Math.atan2(Number.POSITIVE_INFINITY, 0) );

new TestCase( SECTION,
	      "Math.atan2(Infinity, 1)", 
	      Math.PI/2,
	      Math.atan2(Number.POSITIVE_INFINITY, 1) );

new TestCase( SECTION,
	      "Math.atan2(Infinity,-1)", 
	      Math.PI/2,
	      Math.atan2(Number.POSITIVE_INFINITY,-1) );

new TestCase( SECTION,
	      "Math.atan2(Infinity,-0)", 
	      Math.PI/2,
	      Math.atan2(Number.POSITIVE_INFINITY,-0) );

new TestCase( SECTION,
	      "Math.atan2(-Infinity, 0)",
	      -Math.PI/2,
	      Math.atan2(Number.NEGATIVE_INFINITY, 0) );

new TestCase( SECTION,
	      "Math.atan2(-Infinity,-0)",
	      -Math.PI/2,
	      Math.atan2(Number.NEGATIVE_INFINITY,-0) );

new TestCase( SECTION,
	      "Math.atan2(-Infinity, 1)",
	      -Math.PI/2,
	      Math.atan2(Number.NEGATIVE_INFINITY, 1) );

new TestCase( SECTION,
	      "Math.atan2(-Infinity, -1)",
	      -Math.PI/2,
	      Math.atan2(Number.NEGATIVE_INFINITY,-1) );

new TestCase( SECTION,
	      "Math.atan2(Infinity, Infinity)",
	      Math.PI/4,
	      Math.atan2(Number.POSITIVE_INFINITY, Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.atan2(Infinity, -Infinity)", 
	      3*Math.PI/4,
	      Math.atan2(Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.atan2(-Infinity, Infinity)", 
	      -Math.PI/4,
	      Math.atan2(Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.atan2(-Infinity, -Infinity)",
	      -3*Math.PI/4,
	      Math.atan2(Number.NEGATIVE_INFINITY, Number.NEGATIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.atan2(-1, 1)",
	      -Math.PI/4,
	      Math.atan2( -1, 1) );

test();

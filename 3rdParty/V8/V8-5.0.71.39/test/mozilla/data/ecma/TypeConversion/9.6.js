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

gTestfile = '9.6.js';

/**
   File Name:          9.6.js
   ECMA Section:       9.6  Type Conversion:  ToUint32
   Description:        rules for converting an argument to an unsigned
   32 bit integer

   this test uses >>> 0 to convert the argument to
   an unsigned 32bit integer.

   1 call ToNumber on argument
   2 if result is NaN, 0, -0, Infinity, -Infinity
   return 0
   3 compute (sign (result(1)) * floor(abs(result 1)))
   4 compute result(3) modulo 2^32:
   5 return result(4)

   special cases:
   -0          returns 0
   Infinity    returns 0
   -Infinity   returns 0
   0           returns 0
   ToInt32(ToUint32(x)) == ToInt32(x) for all values of x
   ** NEED TO DO THIS PART IN A SEPARATE TEST FILE **


   Author:             christine@netscape.com
   Date:               17 july 1997
*/

var SECTION = "9.6";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Type Conversion:  ToUint32");

new TestCase( SECTION,    "0 >>> 0",                          0,          0 >>> 0 );
//    new TestCase( SECTION,    "+0 >>> 0",                         0,          +0 >>> 0);
new TestCase( SECTION,    "-0 >>> 0",                         0,          -0 >>> 0 );
new TestCase( SECTION,    "'Infinity' >>> 0",                 0,          "Infinity" >>> 0 );
new TestCase( SECTION,    "'-Infinity' >>> 0",                0,          "-Infinity" >>> 0);
new TestCase( SECTION,    "'+Infinity' >>> 0",                0,          "+Infinity" >>> 0 );
new TestCase( SECTION,    "Number.POSITIVE_INFINITY >>> 0",   0,          Number.POSITIVE_INFINITY >>> 0 );
new TestCase( SECTION,    "Number.NEGATIVE_INFINITY >>> 0",   0,          Number.NEGATIVE_INFINITY >>> 0 );
new TestCase( SECTION,    "Number.NaN >>> 0",                 0,          Number.NaN >>> 0 );

new TestCase( SECTION,    "Number.MIN_VALUE >>> 0",           0,          Number.MIN_VALUE >>> 0 );
new TestCase( SECTION,    "-Number.MIN_VALUE >>> 0",          0,          Number.MIN_VALUE >>> 0 );
new TestCase( SECTION,    "0.1 >>> 0",                        0,          0.1 >>> 0 );
new TestCase( SECTION,    "-0.1 >>> 0",                       0,          -0.1 >>> 0 );
new TestCase( SECTION,    "1 >>> 0",                          1,          1 >>> 0 );
new TestCase( SECTION,    "1.1 >>> 0",                        1,          1.1 >>> 0 );

new TestCase( SECTION,    "-1.1 >>> 0",                       ToUint32(-1.1),       -1.1 >>> 0 );
new TestCase( SECTION,    "-1 >>> 0",                         ToUint32(-1),         -1 >>> 0 );

new TestCase( SECTION,    "2147483647 >>> 0",         ToUint32(2147483647),     2147483647 >>> 0 );
new TestCase( SECTION,    "2147483648 >>> 0",         ToUint32(2147483648),     2147483648 >>> 0 );
new TestCase( SECTION,    "2147483649 >>> 0",         ToUint32(2147483649),     2147483649 >>> 0 );

new TestCase( SECTION,    "4294967295 >>> 0",         ToUint32(4294967295),     4294967295 >>> 0 );
new TestCase( SECTION,    "4294967296 >>> 0",         ToUint32(4294967296),     4294967296 >>> 0 );
new TestCase( SECTION,    "4294967297 >>> 0",         ToUint32(4294967297),     4294967297 >>> 0 );

new TestCase( SECTION,    "-2147483647 >>> 0",        ToUint32(-2147483647),    -2147483647 >>> 0 );
new TestCase( SECTION,    "-2147483648 >>> 0",        ToUint32(-2147483648),    -2147483648 >>> 0 );
new TestCase( SECTION,    "-2147483649 >>> 0",        ToUint32(-2147483649),    -2147483649 >>> 0 );

new TestCase( SECTION,    "-4294967295 >>> 0",        ToUint32(-4294967295),    -4294967295 >>> 0 );
new TestCase( SECTION,    "-4294967296 >>> 0",        ToUint32(-4294967296),    -4294967296 >>> 0 );
new TestCase( SECTION,    "-4294967297 >>> 0",        ToUint32(-4294967297),    -4294967297 >>> 0 );

new TestCase( SECTION,    "'2147483647' >>> 0",       ToUint32(2147483647),     '2147483647' >>> 0 );
new TestCase( SECTION,    "'2147483648' >>> 0",       ToUint32(2147483648),     '2147483648' >>> 0 );
new TestCase( SECTION,    "'2147483649' >>> 0",       ToUint32(2147483649),     '2147483649' >>> 0 );

new TestCase( SECTION,    "'4294967295' >>> 0",       ToUint32(4294967295),     '4294967295' >>> 0 );
new TestCase( SECTION,    "'4294967296' >>> 0",       ToUint32(4294967296),     '4294967296' >>> 0 );
new TestCase( SECTION,    "'4294967297' >>> 0",       ToUint32(4294967297),     '4294967297' >>> 0 );


test();

function ToUint32( n ) {
  n = Number( n );
  var sign = ( n < 0 ) ? -1 : 1;

  if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY) {
    return 0;
  }
  n = sign * Math.floor( Math.abs(n) )

    n = n % Math.pow(2,32);

  if ( n < 0 ){
    n += Math.pow(2,32);
  }

  return ( n );
}


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

gTestfile = '11.6.3.js';

/**
   File Name:          11.6.3.js
   ECMA Section:       11.6.3 Applying the additive operators
   (+, -) to numbers
   Description:
   The + operator performs addition when applied to two operands of numeric
   type, producing the sum of the operands. The - operator performs
   subtraction, producing the difference of two numeric operands.

   Addition is a commutative operation, but not always associative.

   The result of an addition is determined using the rules of IEEE 754
   double-precision arithmetic:

   If either operand is NaN, the result is NaN.
   The sum of two infinities of opposite sign is NaN.
   The sum of two infinities of the same sign is the infinity of that sign.
   The sum of an infinity and a finite value is equal to the infinite operand.
   The sum of two negative zeros is 0. The sum of two positive zeros, or of
   two zeros of opposite sign, is +0.
   The sum of a zero and a nonzero finite value is equal to the nonzero
   operand.
   The sum of two nonzero finite values of the same magnitude and opposite
   sign is +0.
   In the remaining cases, where neither an infinity, nor a zero, nor NaN is
   involved, and the operands have the same sign or have different
   magnitudes, the sum is computed and rounded to the nearest
   representable value using IEEE 754 round-to-nearest mode. If the
   magnitude is too large to represent, the operation overflows and
   the result is then an infinity of appropriate sign. The ECMAScript
   language requires support of gradual underflow as defined by IEEE 754.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.6.3";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Applying the additive operators (+,-) to numbers");

new TestCase( SECTION,    "Number.NaN + 1",     Number.NaN,     Number.NaN + 1 );
new TestCase( SECTION,    "1 + Number.NaN",     Number.NaN,     1 + Number.NaN );

new TestCase( SECTION,    "Number.NaN - 1",     Number.NaN,     Number.NaN - 1 );
new TestCase( SECTION,    "1 - Number.NaN",     Number.NaN,     1 - Number.NaN );

new TestCase( SECTION,  "Number.POSITIVE_INFINITY + Number.POSITIVE_INFINITY",  Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY + Number.POSITIVE_INFINITY);
new TestCase( SECTION,  "Number.NEGATIVE_INFINITY + Number.NEGATIVE_INFINITY",  Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY + Number.NEGATIVE_INFINITY);

new TestCase( SECTION,  "Number.POSITIVE_INFINITY + Number.NEGATIVE_INFINITY",  Number.NaN,     Number.POSITIVE_INFINITY + Number.NEGATIVE_INFINITY);
new TestCase( SECTION,  "Number.NEGATIVE_INFINITY + Number.POSITIVE_INFINITY",  Number.NaN,     Number.NEGATIVE_INFINITY + Number.POSITIVE_INFINITY);

new TestCase( SECTION,  "Number.POSITIVE_INFINITY - Number.POSITIVE_INFINITY",  Number.NaN,   Number.POSITIVE_INFINITY - Number.POSITIVE_INFINITY);
new TestCase( SECTION,  "Number.NEGATIVE_INFINITY - Number.NEGATIVE_INFINITY",  Number.NaN,   Number.NEGATIVE_INFINITY - Number.NEGATIVE_INFINITY);

new TestCase( SECTION,  "Number.POSITIVE_INFINITY - Number.NEGATIVE_INFINITY",  Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY - Number.NEGATIVE_INFINITY);
new TestCase( SECTION,  "Number.NEGATIVE_INFINITY - Number.POSITIVE_INFINITY",  Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY - Number.POSITIVE_INFINITY);

new TestCase( SECTION,  "-0 + -0",      -0,     -0 + -0 );
new TestCase( SECTION,  "-0 - 0",       -0,     -0 - 0 );

new TestCase( SECTION,  "0 + 0",        0,      0 + 0 );
new TestCase( SECTION,  "0 + -0",       0,      0 + -0 );
new TestCase( SECTION,  "0 - -0",       0,      0 - -0 );
new TestCase( SECTION,  "0 - 0",        0,      0 - 0 );
new TestCase( SECTION,  "-0 - -0",      0,     -0 - -0 );
new TestCase( SECTION,  "-0 + 0",       0,     -0 + 0 );

new TestCase( SECTION,  "Number.MAX_VALUE - Number.MAX_VALUE",      0,  Number.MAX_VALUE - Number.MAX_VALUE );
new TestCase( SECTION,  "1/Number.MAX_VALUE - 1/Number.MAX_VALUE",  0,  1/Number.MAX_VALUE - 1/Number.MAX_VALUE );

new TestCase( SECTION,  "Number.MIN_VALUE - Number.MIN_VALUE",      0,  Number.MIN_VALUE - Number.MIN_VALUE );

test();

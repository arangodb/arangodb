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

gTestfile = '15.5.4.5-2.js';

/**
   File Name:          15.5.4.5.1.js
   ECMA Section:       15.5.4.5 String.prototype.charCodeAt(pos)
   Description:        Returns a number (a nonnegative integer less than 2^16)
   representing the Unicode encoding of the character at
   position pos in this string.  If there is no character
   at that position, the number is NaN.

   When the charCodeAt method is called with one argument
   pos, the following steps are taken:
   1. Call ToString, giving it the theis value as its
   argument
   2. Call ToInteger(pos)
   3. Compute the number of characters in result(1).
   4. If Result(2) is less than 0 or is not less than
   Result(3), return NaN.
   5. Return a value of Number type, of positive sign, whose
   magnitude is the Unicode encoding of one character
   from result 1, namely the characer at position Result
   (2), where the first character in Result(1) is
   considered to be at position 0.

   Note that the charCodeAt function is intentionally
   generic; it does not require that its this value be a
   String object.  Therefore it can be transferred to other
   kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               2 october 1997
*/
var SECTION = "15.5.4.5-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String.prototype.charCodeAt";

writeHeaderToLog( SECTION + " "+ TITLE);

var TEST_STRING = new String( " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~" );

var x;

new TestCase( SECTION,     "x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(0)", 0x0074,    eval("x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(0)") );
new TestCase( SECTION,     "x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(1)", 0x0072,    eval("x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(1)") );
new TestCase( SECTION,     "x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(2)", 0x0075,    eval("x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(2)") );
new TestCase( SECTION,     "x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(3)", 0x0065,    eval("x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(3)") );
new TestCase( SECTION,     "x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(4)", Number.NaN,     eval("x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(4)") );
new TestCase( SECTION,     "x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(-1)", Number.NaN,    eval("x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(-1)") );

new TestCase( SECTION,     "x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(true)",  0x0072,    eval("x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(true)") );
new TestCase( SECTION,     "x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(false)", 0x0074,    eval("x = new Boolean(true); x.charCodeAt=String.prototype.charCodeAt;x.charCodeAt(false)") );

new TestCase( SECTION,     "x = new String(); x.charCodeAt(0)",    Number.NaN,     eval("x=new String();x.charCodeAt(0)") );
new TestCase( SECTION,     "x = new String(); x.charCodeAt(1)",    Number.NaN,     eval("x=new String();x.charCodeAt(1)") );
new TestCase( SECTION,     "x = new String(); x.charCodeAt(-1)",   Number.NaN,     eval("x=new String();x.charCodeAt(-1)") );

new TestCase( SECTION,     "x = new String(); x.charCodeAt(NaN)",                       Number.NaN,     eval("x=new String();x.charCodeAt(Number.NaN)") );
new TestCase( SECTION,     "x = new String(); x.charCodeAt(Number.POSITIVE_INFINITY)",  Number.NaN,     eval("x=new String();x.charCodeAt(Number.POSITIVE_INFINITY)") );
new TestCase( SECTION,     "x = new String(); x.charCodeAt(Number.NEGATIVE_INFINITY)",  Number.NaN,     eval("x=new String();x.charCodeAt(Number.NEGATIVE_INFINITY)") );

new TestCase( SECTION,  "x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(0)",    0x0031,   eval("x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(0)") );
new TestCase( SECTION,  "x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(1)",    0x002C,   eval("x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(1)") );
new TestCase( SECTION,  "x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(2)",    0x0032,   eval("x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(2)") );
new TestCase( SECTION,  "x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(3)",    0x002C,   eval("x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(3)") );
new TestCase( SECTION,  "x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(4)",    0x0033,   eval("x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(4)") );
new TestCase( SECTION,  "x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(5)",    NaN,   eval("x = new Array(1,2,3); x.charCodeAt = String.prototype.charCodeAt; x.charCodeAt(5)") );

new TestCase( SECTION,  "x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(0)", 0x005B, eval("x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(0)") );
new TestCase( SECTION,  "x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(1)", 0x006F, eval("x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(1)") );
new TestCase( SECTION,  "x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(2)", 0x0062, eval("x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(2)") );
new TestCase( SECTION,  "x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(3)", 0x006A, eval("x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(3)") );
new TestCase( SECTION,  "x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(4)", 0x0065, eval("x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(4)") );
new TestCase( SECTION,  "x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(5)", 0x0063, eval("x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(5)") );
new TestCase( SECTION,  "x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(6)", 0x0074, eval("x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(6)") );

new TestCase( SECTION,  "x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(7)", 0x0020, eval("x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(7)") );

new TestCase( SECTION,  "x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(8)", 0x004F, eval("x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(8)") );
new TestCase( SECTION,  "x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(9)", 0x0062, eval("x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(9)") );
new TestCase( SECTION,  "x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(10)", 0x006A, eval("x = new Function( 'this.charCodeAt = String.prototype.charCodeAt' ); f = new x(); f.charCodeAt(10)") );

test();

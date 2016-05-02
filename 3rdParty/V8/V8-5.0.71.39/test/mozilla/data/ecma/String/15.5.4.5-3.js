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

gTestfile = '15.5.4.5-3.js';

/**
   File Name:          15.5.4.5-3.js
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
var SECTION = "15.5.4.5-3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String.prototype.charCodeAt";

writeHeaderToLog( SECTION + " "+ TITLE);

var TEST_STRING = new String( " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~" );


var foo = new MyObject('hello');

new TestCase( SECTION, "var foo = new MyObject('hello');foo.charCodeAt(0)", 0x0068, foo.charCodeAt(0)  );
new TestCase( SECTION, "var foo = new MyObject('hello');foo.charCodeAt(1)", 0x0065, foo.charCodeAt(1)  );
new TestCase( SECTION, "var foo = new MyObject('hello');foo.charCodeAt(2)", 0x006c, foo.charCodeAt(2)  );
new TestCase( SECTION, "var foo = new MyObject('hello');foo.charCodeAt(3)", 0x006c, foo.charCodeAt(3)  );
new TestCase( SECTION, "var foo = new MyObject('hello');foo.charCodeAt(4)", 0x006f, foo.charCodeAt(4)  );
new TestCase( SECTION, "var foo = new MyObject('hello');foo.charCodeAt(-1)", Number.NaN,  foo.charCodeAt(-1)  );
new TestCase( SECTION, "var foo = new MyObject('hello');foo.charCodeAt(5)", Number.NaN,  foo.charCodeAt(5)  );

var boo = new MyObject(true);

new TestCase( SECTION, "var boo = new MyObject(true);boo.charCodeAt(0)", 0x0074, boo.charCodeAt(0)  );
new TestCase( SECTION, "var boo = new MyObject(true);boo.charCodeAt(1)", 0x0072, boo.charCodeAt(1)  );
new TestCase( SECTION, "var boo = new MyObject(true);boo.charCodeAt(2)", 0x0075, boo.charCodeAt(2)  );
new TestCase( SECTION, "var boo = new MyObject(true);boo.charCodeAt(3)", 0x0065, boo.charCodeAt(3)  );

var noo = new MyObject( Math.PI );

new TestCase( SECTION, "var noo = new MyObject(Math.PI);noo.charCodeAt(0)", 0x0033, noo.charCodeAt(0)  );
new TestCase( SECTION, "var noo = new MyObject(Math.PI);noo.charCodeAt(1)", 0x002E, noo.charCodeAt(1)  );
new TestCase( SECTION, "var noo = new MyObject(Math.PI);noo.charCodeAt(2)", 0x0031, noo.charCodeAt(2)  );
new TestCase( SECTION, "var noo = new MyObject(Math.PI);noo.charCodeAt(3)", 0x0034, noo.charCodeAt(3)  );
new TestCase( SECTION, "var noo = new MyObject(Math.PI);noo.charCodeAt(4)", 0x0031, noo.charCodeAt(4)  );
new TestCase( SECTION, "var noo = new MyObject(Math.PI);noo.charCodeAt(5)", 0x0035, noo.charCodeAt(5)  );
new TestCase( SECTION, "var noo = new MyObject(Math.PI);noo.charCodeAt(6)", 0x0039, noo.charCodeAt(6)  );

var noo = new MyObject( null );

new TestCase( SECTION, "var noo = new MyObject(null);noo.charCodeAt(0)", 0x006E, noo.charCodeAt(0)  );
new TestCase( SECTION, "var noo = new MyObject(null);noo.charCodeAt(1)", 0x0075, noo.charCodeAt(1)  );
new TestCase( SECTION, "var noo = new MyObject(null);noo.charCodeAt(2)", 0x006C, noo.charCodeAt(2)  );
new TestCase( SECTION, "var noo = new MyObject(null);noo.charCodeAt(3)", 0x006C, noo.charCodeAt(3)  );
new TestCase( SECTION, "var noo = new MyObject(null);noo.charCodeAt(4)", NaN, noo.charCodeAt(4)  );

var noo = new MyObject( void 0 );

new TestCase( SECTION, "var noo = new MyObject(void 0);noo.charCodeAt(0)", 0x0075, noo.charCodeAt(0)  );
new TestCase( SECTION, "var noo = new MyObject(void 0);noo.charCodeAt(1)", 0x006E, noo.charCodeAt(1)  );
new TestCase( SECTION, "var noo = new MyObject(void 0);noo.charCodeAt(2)", 0x0064, noo.charCodeAt(2)  );
new TestCase( SECTION, "var noo = new MyObject(void 0);noo.charCodeAt(3)", 0x0065, noo.charCodeAt(3)  );
new TestCase( SECTION, "var noo = new MyObject(void 0);noo.charCodeAt(4)", 0x0066, noo.charCodeAt(4)  );

test();


function MyObject (v) {
  this.value      = v;
  this.toString   = new Function ( "return this.value +\"\"" );
  this.charCodeAt     = String.prototype.charCodeAt;
}

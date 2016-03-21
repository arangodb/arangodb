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

gTestfile = '11.9.1.js';

/**
   File Name:          11.9.1.js
   ECMA Section:       11.9.1 The equals operator ( == )
   Description:

   The production EqualityExpression:
   EqualityExpression ==  RelationalExpression is evaluated as follows:

   1.  Evaluate EqualityExpression.
   2.  Call GetValue(Result(1)).
   3.  Evaluate RelationalExpression.
   4.  Call GetValue(Result(3)).
   5.  Perform the comparison Result(4) == Result(2). (See section 11.9.3)
   6.  Return Result(5).
   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.9.1";
var VERSION = "ECMA_1";
var BUGNUMBER="77391";
startTest();

writeHeaderToLog( SECTION + " The equals operator ( == )");

// type x and type y are the same.  if type x is undefined or null, return true

new TestCase( SECTION,    "void 0 = void 0",        true,   void 0 == void 0 );
new TestCase( SECTION,    "null == null",           true,   null == null );

//  if x is NaN, return false. if y is NaN, return false.

new TestCase( SECTION,    "NaN == NaN",             false,  Number.NaN == Number.NaN );
new TestCase( SECTION,    "NaN == 0",               false,  Number.NaN == 0 );
new TestCase( SECTION,    "0 == NaN",               false,  0 == Number.NaN );
new TestCase( SECTION,    "NaN == Infinity",        false,  Number.NaN == Number.POSITIVE_INFINITY );
new TestCase( SECTION,    "Infinity == NaN",        false,  Number.POSITIVE_INFINITY == Number.NaN );

// if x is the same number value as y, return true.

new TestCase( SECTION,    "Number.MAX_VALUE == Number.MAX_VALUE",   true,   Number.MAX_VALUE == Number.MAX_VALUE );
new TestCase( SECTION,    "Number.MIN_VALUE == Number.MIN_VALUE",   true,   Number.MIN_VALUE == Number.MIN_VALUE );
new TestCase( SECTION,    "Number.POSITIVE_INFINITY == Number.POSITIVE_INFINITY",   true,   Number.POSITIVE_INFINITY == Number.POSITIVE_INFINITY );
new TestCase( SECTION,    "Number.NEGATIVE_INFINITY == Number.NEGATIVE_INFINITY",   true,   Number.NEGATIVE_INFINITY == Number.NEGATIVE_INFINITY );

//  if xis 0 and y is -0, return true.   if x is -0 and y is 0, return true.

new TestCase( SECTION,    "0 == 0",                 true,   0 == 0 );
new TestCase( SECTION,    "0 == -0",                true,   0 == -0 );
new TestCase( SECTION,    "-0 == 0",                true,   -0 == 0 );
new TestCase( SECTION,    "-0 == -0",               true,   -0 == -0 );

// return false.

new TestCase( SECTION,    "0.9 == 1",               false,  0.9 == 1 );
new TestCase( SECTION,    "0.999999 == 1",          false,  0.999999 == 1 );
new TestCase( SECTION,    "0.9999999999 == 1",      false,  0.9999999999 == 1 );
new TestCase( SECTION,    "0.9999999999999 == 1",   false,  0.9999999999999 == 1 );

// type x and type y are the same type, but not numbers.


// x and y are strings.  return true if x and y are exactly the same sequence of characters.
// otherwise, return false.

new TestCase( SECTION,    "'hello' == 'hello'",         true,   "hello" == "hello" );

// x and y are booleans.  return true if both are true or both are false.

new TestCase( SECTION,    "true == true",               true,   true == true );
new TestCase( SECTION,    "false == false",             true,   false == false );
new TestCase( SECTION,    "true == false",              false,  true == false );
new TestCase( SECTION,    "false == true",              false,  false == true );

// return true if x and y refer to the same object.  otherwise return false.

new TestCase( SECTION,    "new MyObject(true) == new MyObject(true)",   false,  new MyObject(true) == new MyObject(true) );
new TestCase( SECTION,    "new Boolean(true) == new Boolean(true)",     false,  new Boolean(true) == new Boolean(true) );
new TestCase( SECTION,    "new Boolean(false) == new Boolean(false)",   false,  new Boolean(false) == new Boolean(false) );


new TestCase( SECTION,    "x = new MyObject(true); y = x; z = x; z == y",   true,  eval("x = new MyObject(true); y = x; z = x; z == y") );
new TestCase( SECTION,    "x = new MyObject(false); y = x; z = x; z == y",  true,  eval("x = new MyObject(false); y = x; z = x; z == y") );
new TestCase( SECTION,    "x = new Boolean(true); y = x; z = x; z == y",   true,  eval("x = new Boolean(true); y = x; z = x; z == y") );
new TestCase( SECTION,    "x = new Boolean(false); y = x; z = x; z == y",   true,  eval("x = new Boolean(false); y = x; z = x; z == y") );

new TestCase( SECTION,    "new Boolean(true) == new Boolean(true)",     false,  new Boolean(true) == new Boolean(true) );
new TestCase( SECTION,    "new Boolean(false) == new Boolean(false)",   false,  new Boolean(false) == new Boolean(false) );

// if x is null and y is undefined, return true.  if x is undefined and y is null return true.

new TestCase( SECTION,    "null == void 0",             true,   null == void 0 );
new TestCase( SECTION,    "void 0 == null",             true,   void 0 == null );

// if type(x) is Number and type(y) is string, return the result of the comparison x == ToNumber(y).

new TestCase( SECTION,    "1 == '1'",                   true,   1 == '1' );
new TestCase( SECTION,    "255 == '0xff'",               true,  255 == '0xff' );
new TestCase( SECTION,    "0 == '\r'",                  true,   0 == "\r" );
new TestCase( SECTION,    "1e19 == '1e19'",             true,   1e19 == "1e19" );


new TestCase( SECTION,    "new Boolean(true) == true",  true,   true == new Boolean(true) );
new TestCase( SECTION,    "new MyObject(true) == true", true,   true == new MyObject(true) );

new TestCase( SECTION,    "new Boolean(false) == false",    true,   new Boolean(false) == false );
new TestCase( SECTION,    "new MyObject(false) == false",   true,   new MyObject(false) == false );

new TestCase( SECTION,    "true == new Boolean(true)",      true,   true == new Boolean(true) );
new TestCase( SECTION,    "true == new MyObject(true)",     true,   true == new MyObject(true) );

new TestCase( SECTION,    "false == new Boolean(false)",    true,   false == new Boolean(false) );
new TestCase( SECTION,    "false == new MyObject(false)",   true,   false == new MyObject(false) );

test();

function MyObject( value ) {
  this.value = value;
  this.valueOf = new Function( "return this.value" );
}

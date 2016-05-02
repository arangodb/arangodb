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
 * Contributor(s): christine@netscape.com
 *                 Jesse Ruderman
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

gTestfile = '9.2.js';

/**
   File Name:          9.2.js
   ECMA Section:       9.2  Type Conversion:  ToBoolean
   Description:        rules for converting an argument to a boolean.
   undefined           false
   Null                false
   Boolean             input argument( no conversion )
   Number              returns false for 0, -0, and NaN
   otherwise return true
   String              return false if the string is empty
   (length is 0) otherwise the result is
   true
   Object              all return true

   Author:             christine@netscape.com
   Date:               14 july 1997
*/
var SECTION = "9.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "ToBoolean";

writeHeaderToLog( SECTION + " "+ TITLE);

// special cases here

new TestCase( SECTION,   "Boolean()",                     false,  Boolean() );
new TestCase( SECTION,   "Boolean(var x)",                false,  Boolean(eval("var x")) );
new TestCase( SECTION,   "Boolean(void 0)",               false,  Boolean(void 0) );
new TestCase( SECTION,   "Boolean(null)",                 false,  Boolean(null) );
new TestCase( SECTION,   "Boolean(false)",                false,  Boolean(false) );
new TestCase( SECTION,   "Boolean(true)",                 true,   Boolean(true) );
new TestCase( SECTION,   "Boolean(0)",                    false,  Boolean(0) );
new TestCase( SECTION,   "Boolean(-0)",                   false,  Boolean(-0) );
new TestCase( SECTION,   "Boolean(NaN)",                  false,  Boolean(Number.NaN) );
new TestCase( SECTION,   "Boolean('')",                   false,  Boolean("") );

// normal test cases here

new TestCase( SECTION,   "Boolean(Infinity)",             true,   Boolean(Number.POSITIVE_INFINITY) );
new TestCase( SECTION,   "Boolean(-Infinity)",            true,   Boolean(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION,   "Boolean(Math.PI)",              true,   Boolean(Math.PI) );
new TestCase( SECTION,   "Boolean(1)",                    true,   Boolean(1) );
new TestCase( SECTION,   "Boolean(-1)",                   true,   Boolean(-1) );
new TestCase( SECTION,   "Boolean([tab])",                true,   Boolean("\t") );
new TestCase( SECTION,   "Boolean('0')",                  true,   Boolean("0") );
new TestCase( SECTION,   "Boolean('string')",             true,   Boolean("string") );

// ToBoolean (object) should always return true.
new TestCase( SECTION,   "Boolean(new String() )",        true,   Boolean(new String()) );
new TestCase( SECTION,   "Boolean(new String('') )",      true,   Boolean(new String("")) );

new TestCase( SECTION,   "Boolean(new Boolean(true))",    true,   Boolean(new Boolean(true)) );
new TestCase( SECTION,   "Boolean(new Boolean(false))",   true,   Boolean(new Boolean(false)) );
new TestCase( SECTION,   "Boolean(new Boolean() )",       true,   Boolean(new Boolean()) );

new TestCase( SECTION,   "Boolean(new Array())",          true,   Boolean(new Array()) );

new TestCase( SECTION,   "Boolean(new Number())",         true,   Boolean(new Number()) );
new TestCase( SECTION,   "Boolean(new Number(-0))",       true,   Boolean(new Number(-0)) );
new TestCase( SECTION,   "Boolean(new Number(0))",        true,   Boolean(new Number(0)) );
new TestCase( SECTION,   "Boolean(new Number(NaN))",      true,   Boolean(new Number(Number.NaN)) );

new TestCase( SECTION,   "Boolean(new Number(-1))",       true,   Boolean(new Number(-1)) );
new TestCase( SECTION,   "Boolean(new Number(Infinity))", true,   Boolean(new Number(Number.POSITIVE_INFINITY)) );
new TestCase( SECTION,   "Boolean(new Number(-Infinity))",true,   Boolean(new Number(Number.NEGATIVE_INFINITY)) );

new TestCase( SECTION,    "Boolean(new Object())",       true,       Boolean(new Object()) );
new TestCase( SECTION,    "Boolean(new Function())",     true,       Boolean(new Function()) );
new TestCase( SECTION,    "Boolean(new Date())",         true,       Boolean(new Date()) );
new TestCase( SECTION,    "Boolean(new Date(0))",         true,       Boolean(new Date(0)) );
new TestCase( SECTION,    "Boolean(Math)",         true,       Boolean(Math) );

// bug 375793
new TestCase( SECTION,
              "NaN ? true : false",
              false,
              (NaN ? true : false) );
new TestCase( SECTION,
              "1000 % 0 ? true : false",
              false,
              (1000 % 0 ? true : false) );
new TestCase( SECTION,
              "(function(a,b){ return a % b ? true : false })(1000, 0)",
              false,
              ((function(a,b){ return a % b ? true : false })(1000, 0)) );

new TestCase( SECTION,
              "(function(x) { return !(x) })(0/0)",
              true,
              ((function(x) { return !(x) })(0/0)) );
new TestCase( SECTION,
              "!(0/0)",
              true,
              (!(0/0)) );
test();


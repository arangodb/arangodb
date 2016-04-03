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

gTestfile = '9.9-1.js';

/**
   File Name:          9.9-1.js
   ECMA Section:       9.9  Type Conversion:  ToObject
   Description:

   undefined   generate a runtime error
   null        generate a runtime error
   boolean     create a new Boolean object whose default
   value is the value of the boolean.
   number      Create a new Number object whose default
   value is the value of the number.
   string      Create a new String object whose default
   value is the value of the string.
   object      Return the input argument (no conversion).
   Author:             christine@netscape.com
   Date:               17 july 1997
*/

var VERSION = "ECMA_1";
startTest();
var SECTION = "9.9-1";

writeHeaderToLog( SECTION + " Type Conversion: ToObject" );

new TestCase( SECTION, "(Object(true)).__proto__",  Boolean.prototype,      (Object(true)).__proto__ );

new TestCase( SECTION, "(Object(true)).__proto__",  Boolean.prototype,      (Object(true)).__proto__ );

new TestCase( SECTION, "(Object(0)).__proto__",     Number.prototype,      (Object(0)).__proto__ );

new TestCase( SECTION, "(Object(-0)).__proto__",    Number.prototype,      (Object(-0)).__proto__ );

new TestCase( SECTION, "(Object(1)).__proto__",     Number.prototype,      (Object(1)).__proto__ );

new TestCase( SECTION, "(Object(-1)).__proto__",    Number.prototype,      (Object(-1)).__proto__ );

new TestCase( SECTION, "(Object(Number.MAX_VALUE)).__proto__",  Number.prototype,               (Object(Number.MAX_VALUE)).__proto__ );

new TestCase( SECTION, "(Object(Number.MIN_VALUE)).__proto__",  Number.prototype, (Object(Number.MIN_VALUE)).__proto__ );

new TestCase( SECTION, "(Object(Number.POSITIVE_INFINITY)).__proto__",  Number.prototype,               (Object(Number.POSITIVE_INFINITY)).__proto__ );

new TestCase( SECTION, "(Object(Number.NEGATIVE_INFINITY)).__proto__",  Number.prototype,   (Object(Number.NEGATIVE_INFINITY)).__proto__ );

new TestCase( SECTION, "(Object(Number.NaN)).__proto__",    Number.prototype,          (Object(Number.NaN)).__proto__ );

new TestCase( SECTION, "(Object('a string')).__proto__",    String.prototype,   (Object("a string")).__proto__ );

new TestCase( SECTION, "(Object('')).__proto__",            String.prototype,   (Object("")).__proto__ );

new TestCase( SECTION, "(Object('\\r\\t\\b\\n\\v\\f')).__proto__", String.prototype,   (Object("\\r\\t\\b\\n\\v\\f")).__proto__ );

new TestCase( SECTION,  "Object( '\\\'\\\"\\' ).__proto__",      String.prototype,   (Object("\'\"\\")).__proto__ );

new TestCase( SECTION, "(Object( new MyObject(true) )).toString()",  "[object Object]",       eval("(Object( new MyObject(true) )).toString()") );

test();

function MyObject( value ) {
  this.value = value;
  this.valueOf = new Function ( "return this.value" );
}

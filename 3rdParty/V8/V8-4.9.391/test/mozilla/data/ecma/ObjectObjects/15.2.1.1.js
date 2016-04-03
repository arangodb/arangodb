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

gTestfile = '15.2.1.1.js';

/**
   File Name:          15.2.1.1.js
   ECMA Section:       15.2.1.1  The Object Constructor Called as a Function:
   Object(value)
   Description:        When Object is called as a function rather than as a
   constructor, the following steps are taken:

   1.  If value is null or undefined, create and return a
   new object with no properties other than internal
   properties exactly as if the object constructor
   had been called on that same value (15.2.2.1).
   2.  Return ToObject (value), whose rules are:

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

var SECTION = "15.2.1.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Object( value )";

writeHeaderToLog( SECTION + " "+ TITLE);


var NULL_OBJECT = Object(null);

new TestCase( SECTION, "Object(null).valueOf()",    NULL_OBJECT,           (NULL_OBJECT).valueOf() );
new TestCase( SECTION, "typeof Object(null)",       "object",               typeof (Object(null)) );

var UNDEFINED_OBJECT = Object( void 0 );

new TestCase( SECTION, "Object(void 0).valueOf()",    UNDEFINED_OBJECT,           (UNDEFINED_OBJECT).valueOf() );
new TestCase( SECTION, "typeof Object(void 0)",       "object",               typeof (Object(void 0)) );

new TestCase( SECTION, "Object(true).valueOf()",    true,                   (Object(true)).valueOf() );
new TestCase( SECTION, "typeof Object(true)",       "object",               typeof Object(true) );
new TestCase( SECTION, "var MYOB = Object(true); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "[object Boolean]",      eval("var MYOB = Object(true); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object(false).valueOf()",    false,                  (Object(false)).valueOf() );
new TestCase( SECTION, "typeof Object(false)",      "object",               typeof Object(false) );
new TestCase( SECTION, "var MYOB = Object(false); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "[object Boolean]",      eval("var MYOB = Object(false); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object(0).valueOf()",       0,                      (Object(0)).valueOf() );
new TestCase( SECTION, "typeof Object(0)",          "object",               typeof Object(0) );
new TestCase( SECTION, "var MYOB = Object(0); MYOB.toString = Object.prototype.toString; MYOB.toString()",      "[object Number]",      eval("var MYOB = Object(0); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object(-0).valueOf()",      -0,                     (Object(-0)).valueOf() );
new TestCase( SECTION, "typeof Object(-0)",         "object",               typeof Object(-0) );
new TestCase( SECTION, "var MYOB = Object(-0); MYOB.toString = Object.prototype.toString; MYOB.toString()",     "[object Number]",      eval("var MYOB = Object(-0); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object(1).valueOf()",       1,                      (Object(1)).valueOf() );
new TestCase( SECTION, "typeof Object(1)",          "object",               typeof Object(1) );
new TestCase( SECTION, "var MYOB = Object(1); MYOB.toString = Object.prototype.toString; MYOB.toString()",     "[object Number]",      eval("var MYOB = Object(1); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object(-1).valueOf()",      -1,                     (Object(-1)).valueOf() );
new TestCase( SECTION, "typeof Object(-1)",         "object",               typeof Object(-1) );
new TestCase( SECTION, "var MYOB = Object(-1); MYOB.toString = Object.prototype.toString; MYOB.toString()",     "[object Number]",      eval("var MYOB = Object(-1); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object(Number.MAX_VALUE).valueOf()",    1.7976931348623157e308,         (Object(Number.MAX_VALUE)).valueOf() );
new TestCase( SECTION, "typeof Object(Number.MAX_VALUE)",       "object",                       typeof Object(Number.MAX_VALUE) );
new TestCase( SECTION, "var MYOB = Object(Number.MAX_VALUE); MYOB.toString = Object.prototype.toString; MYOB.toString()",     "[object Number]",      eval("var MYOB = Object(Number.MAX_VALUE); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object(Number.MIN_VALUE).valueOf()",     5e-324,           (Object(Number.MIN_VALUE)).valueOf() );
new TestCase( SECTION, "typeof Object(Number.MIN_VALUE)",       "object",         typeof Object(Number.MIN_VALUE) );
new TestCase( SECTION, "var MYOB = Object(Number.MIN_VALUE); MYOB.toString = Object.prototype.toString; MYOB.toString()",     "[object Number]",      eval("var MYOB = Object(Number.MIN_VALUE); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object(Number.POSITIVE_INFINITY).valueOf()",    Number.POSITIVE_INFINITY,       (Object(Number.POSITIVE_INFINITY)).valueOf() );
new TestCase( SECTION, "typeof Object(Number.POSITIVE_INFINITY)",       "object",                       typeof Object(Number.POSITIVE_INFINITY) );
new TestCase( SECTION, "var MYOB = Object(Number.POSITIVE_INFINITY); MYOB.toString = Object.prototype.toString; MYOB.toString()",     "[object Number]",      eval("var MYOB = Object(Number.POSITIVE_INFINITY); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object(Number.NEGATIVE_INFINITY).valueOf()",    Number.NEGATIVE_INFINITY,       (Object(Number.NEGATIVE_INFINITY)).valueOf() );
new TestCase( SECTION, "typeof Object(Number.NEGATIVE_INFINITY)",       "object",            typeof Object(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION, "var MYOB = Object(Number.NEGATIVE_INFINITY); MYOB.toString = Object.prototype.toString; MYOB.toString()",     "[object Number]",      eval("var MYOB = Object(Number.NEGATIVE_INFINITY); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object(Number.NaN).valueOf()",      Number.NaN,                (Object(Number.NaN)).valueOf() );
new TestCase( SECTION, "typeof Object(Number.NaN)",         "object",                  typeof Object(Number.NaN) );
new TestCase( SECTION, "var MYOB = Object(Number.NaN); MYOB.toString = Object.prototype.toString; MYOB.toString()",     "[object Number]",      eval("var MYOB = Object(Number.NaN); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object('a string').valueOf()",      "a string",         (Object("a string")).valueOf() );
new TestCase( SECTION, "typeof Object('a string')",         "object",           typeof (Object("a string")) );
new TestCase( SECTION, "var MYOB = Object('a string'); MYOB.toString = Object.prototype.toString; MYOB.toString()",     "[object String]",      eval("var MYOB = Object('a string'); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object('').valueOf()",              "",                 (Object("")).valueOf() );
new TestCase( SECTION, "typeof Object('')",                 "object",           typeof (Object("")) );
new TestCase( SECTION, "var MYOB = Object(''); MYOB.toString = Object.prototype.toString; MYOB.toString()",     "[object String]",      eval("var MYOB = Object(''); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION, "Object('\\r\\t\\b\\n\\v\\f').valueOf()",   "\r\t\b\n\v\f",   (Object("\r\t\b\n\v\f")).valueOf() );
new TestCase( SECTION, "typeof Object('\\r\\t\\b\\n\\v\\f')",      "object",           typeof (Object("\\r\\t\\b\\n\\v\\f")) );
new TestCase( SECTION, "var MYOB = Object('\\r\\t\\b\\n\\v\\f'); MYOB.toString = Object.prototype.toString; MYOB.toString()",     "[object String]",      eval("var MYOB = Object('\\r\\t\\b\\n\\v\\f'); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

new TestCase( SECTION,  "Object( '\\\'\\\"\\' ).valueOf()",      "\'\"\\",          (Object("\'\"\\")).valueOf() );
new TestCase( SECTION,  "typeof Object( '\\\'\\\"\\' )",        "object",           typeof Object("\'\"\\") );
//    new TestCase( SECTION, "var MYOB = Object(  '\\\'\\\"\\' ); MYOB.toString = Object.prototype.toString; MYOB.toString()",     "[object String]",      eval("var MYOB = Object( '\\\'\\\"\\' ); MYOB.toString = Object.prototype.toString; MYOB.toString()") );

test();

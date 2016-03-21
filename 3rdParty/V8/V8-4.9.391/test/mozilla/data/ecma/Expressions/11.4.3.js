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

gTestfile = '11.4.3.js';

/**
   File Name:          typeof_1.js
   ECMA Section:       11.4.3 typeof operator
   Description:        typeof evaluates unary expressions:
   undefined   "undefined"
   null        "object"
   Boolean     "boolean"
   Number      "number"
   String      "string"
   Object      "object" [native, doesn't implement Call]
   Object      "function" [native, implements [Call]]
   Object      implementation dependent
   [not sure how to test this]
   Author:             christine@netscape.com
   Date:               june 30, 1997

*/

var SECTION = "11.4.3";

var VERSION = "ECMA_1";
startTest();
var TITLE   = " The typeof operator";
writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,     "typeof(void(0))",              "undefined",        typeof(void(0)) );
new TestCase( SECTION,     "typeof(null)",                 "object",           typeof(null) );
new TestCase( SECTION,     "typeof(true)",                 "boolean",          typeof(true) );
new TestCase( SECTION,     "typeof(false)",                "boolean",          typeof(false) );
new TestCase( SECTION,     "typeof(new Boolean())",        "object",           typeof(new Boolean()) );
new TestCase( SECTION,     "typeof(new Boolean(true))",    "object",           typeof(new Boolean(true)) );
new TestCase( SECTION,     "typeof(Boolean())",            "boolean",          typeof(Boolean()) );
new TestCase( SECTION,     "typeof(Boolean(false))",       "boolean",          typeof(Boolean(false)) );
new TestCase( SECTION,     "typeof(Boolean(true))",        "boolean",          typeof(Boolean(true)) );
new TestCase( SECTION,     "typeof(NaN)",                  "number",           typeof(Number.NaN) );
new TestCase( SECTION,     "typeof(Infinity)",             "number",           typeof(Number.POSITIVE_INFINITY) );
new TestCase( SECTION,     "typeof(-Infinity)",            "number",           typeof(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION,     "typeof(Math.PI)",              "number",           typeof(Math.PI) );
new TestCase( SECTION,     "typeof(0)",                    "number",           typeof(0) );
new TestCase( SECTION,     "typeof(1)",                    "number",           typeof(1) );
new TestCase( SECTION,     "typeof(-1)",                   "number",           typeof(-1) );
new TestCase( SECTION,     "typeof('0')",                  "string",           typeof("0") );
new TestCase( SECTION,     "typeof(Number())",             "number",           typeof(Number()) );
new TestCase( SECTION,     "typeof(Number(0))",            "number",           typeof(Number(0)) );
new TestCase( SECTION,     "typeof(Number(1))",            "number",           typeof(Number(1)) );
new TestCase( SECTION,     "typeof(Nubmer(-1))",           "number",           typeof(Number(-1)) );
new TestCase( SECTION,     "typeof(new Number())",         "object",           typeof(new Number()) );
new TestCase( SECTION,     "typeof(new Number(0))",        "object",           typeof(new Number(0)) );
new TestCase( SECTION,     "typeof(new Number(1))",        "object",           typeof(new Number(1)) );

// Math does not implement [[Construct]] or [[Call]] so its type is object.

new TestCase( SECTION,     "typeof(Math)",                 "object",         typeof(Math) );

new TestCase( SECTION,     "typeof(Number.prototype.toString)", "function",    typeof(Number.prototype.toString) );

new TestCase( SECTION,     "typeof('a string')",           "string",           typeof("a string") );
new TestCase( SECTION,     "typeof('')",                   "string",           typeof("") );
new TestCase( SECTION,     "typeof(new Date())",           "object",           typeof(new Date()) );
new TestCase( SECTION,     "typeof(new Array(1,2,3))",     "object",           typeof(new Array(1,2,3)) );
new TestCase( SECTION,     "typeof(new String('string object'))",  "object",   typeof(new String("string object")) );
new TestCase( SECTION,     "typeof(String('string primitive'))",    "string",  typeof(String("string primitive")) );
new TestCase( SECTION,     "typeof(['array', 'of', 'strings'])",   "object",   typeof(["array", "of", "strings"]) );
new TestCase( SECTION,     "typeof(new Function())",                "function",     typeof( new Function() ) );
new TestCase( SECTION,     "typeof(parseInt)",                      "function",     typeof( parseInt ) );
new TestCase( SECTION,     "typeof(test)",                          "function",     typeof( test ) );
new TestCase( SECTION,     "typeof(String.fromCharCode)",           "function",     typeof( String.fromCharCode )  );


test();


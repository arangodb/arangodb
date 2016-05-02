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

gTestfile = '15.1.2.1-1.js';

/**
   File Name:          15.1.2.1-1.js
   ECMA Section:       15.1.2.1 eval(x)

   if x is not a string object, return x.
   Description:
   Author:             christine@netscape.com
   Date:               16 september 1997
*/
var SECTION = "15.1.2.1-1";
var VERSION = "ECMA_1";
var TITLE   = "eval(x)";
var BUGNUMBER = "none";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,      "eval.length",              1,              eval.length );
new TestCase( SECTION,      "delete eval.length",       false,          delete eval.length );
new TestCase( SECTION,      "var PROPS = ''; for ( p in eval ) { PROPS += p }; PROPS",  "prototype", eval("var PROPS = ''; for ( p in eval ) { PROPS += p }; PROPS") );
new TestCase( SECTION,      "eval.length = null; eval.length",       1, eval( "eval.length = null; eval.length") );
//     new TestCase( SECTION,     "eval.__proto__",                       Function.prototype,            eval.__proto__ );

// test cases where argument is not a string.  should return the argument.

new TestCase( SECTION,     "eval()",                                void 0,                     eval() );
new TestCase( SECTION,     "eval(void 0)",                          void 0,                     eval( void 0) );
new TestCase( SECTION,     "eval(null)",                            null,                       eval( null ) );
new TestCase( SECTION,     "eval(true)",                            true,                       eval( true ) );
new TestCase( SECTION,     "eval(false)",                           false,                      eval( false ) );

new TestCase( SECTION,     "typeof eval(new String('Infinity/-0')", "object",                   typeof eval(new String('Infinity/-0')) );

new TestCase( SECTION,     "eval([1,2,3,4,5,6])",                  "1,2,3,4,5,6",                 ""+eval([1,2,3,4,5,6]) );
new TestCase( SECTION,     "eval(new Array(0,1,2,3)",              "1,2,3",                       ""+  eval(new Array(1,2,3)) );
new TestCase( SECTION,     "eval(1)",                              1,                             eval(1) );
new TestCase( SECTION,     "eval(0)",                              0,                             eval(0) );
new TestCase( SECTION,     "eval(-1)",                             -1,                            eval(-1) );
new TestCase( SECTION,     "eval(Number.NaN)",                     Number.NaN,                    eval(Number.NaN) );
new TestCase( SECTION,     "eval(Number.MIN_VALUE)",               5e-308,                        eval(Number.MIN_VALUE) );
new TestCase( SECTION,     "eval(-Number.MIN_VALUE)",              -5e-308,                       eval(-Number.MIN_VALUE) );
new TestCase( SECTION,     "eval(Number.POSITIVE_INFINITY)",       Number.POSITIVE_INFINITY,      eval(Number.POSITIVE_INFINITY) );
new TestCase( SECTION,     "eval(Number.NEGATIVE_INFINITY)",       Number.NEGATIVE_INFINITY,      eval(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION,     "eval( 4294967296 )",                   4294967296,                    eval(4294967296) );
new TestCase( SECTION,     "eval( 2147483648 )",                   2147483648,                    eval(2147483648) );

test();

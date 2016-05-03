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

gTestfile = '11.4.9.js';

/**
   File Name:          11.4.9.js
   ECMA Section:       11.4.9 Logical NOT Operator (!)
   Description:        if the ToBoolean( VALUE ) result is true, return
   true.  else return false.
   Author:             christine@netscape.com
   Date:               7 july 1997

   Static variables:
   none
*/
var SECTION = "11.4.9";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Logical NOT operator (!)";

writeHeaderToLog( SECTION + " "+ TITLE);

//    version("130")


new TestCase( SECTION,   "!(null)",                true,   !(null) );
new TestCase( SECTION,   "!(var x)",               true,   !(eval("var x")) );
new TestCase( SECTION,   "!(void 0)",              true,   !(void 0) );

new TestCase( SECTION,   "!(false)",               true,   !(false) );
new TestCase( SECTION,   "!(true)",                false,  !(true) );
new TestCase( SECTION,   "!()",                    true,   !(eval()) );
new TestCase( SECTION,   "!(0)",                   true,   !(0) );
new TestCase( SECTION,   "!(-0)",                  true,   !(-0) );
new TestCase( SECTION,   "!(NaN)",                 true,   !(Number.NaN) );
new TestCase( SECTION,   "!(Infinity)",            false,  !(Number.POSITIVE_INFINITY) );
new TestCase( SECTION,   "!(-Infinity)",           false,  !(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION,   "!(Math.PI)",             false,  !(Math.PI) );
new TestCase( SECTION,   "!(1)",                   false,  !(1) );
new TestCase( SECTION,   "!(-1)",                  false,  !(-1) );
new TestCase( SECTION,   "!('')",                  true,   !("") );
new TestCase( SECTION,   "!('\t')",                false,  !("\t") );
new TestCase( SECTION,   "!('0')",                 false,  !("0") );
new TestCase( SECTION,   "!('string')",            false,  !("string") );
new TestCase( SECTION,   "!(new String(''))",      false,  !(new String("")) );
new TestCase( SECTION,   "!(new String('string'))",    false,  !(new String("string")) );
new TestCase( SECTION,   "!(new String())",        false,  !(new String()) );
new TestCase( SECTION,   "!(new Boolean(true))",   false,   !(new Boolean(true)) );
new TestCase( SECTION,   "!(new Boolean(false))",  false,   !(new Boolean(false)) );
new TestCase( SECTION,   "!(new Array())",         false,  !(new Array()) );
new TestCase( SECTION,   "!(new Array(1,2,3)",     false,  !(new Array(1,2,3)) );
new TestCase( SECTION,   "!(new Number())",        false,  !(new Number()) );
new TestCase( SECTION,   "!(new Number(0))",       false,  !(new Number(0)) );
new TestCase( SECTION,   "!(new Number(NaN))",     false,  !(new Number(Number.NaN)) );
new TestCase( SECTION,   "!(new Number(Infinity))", false, !(new Number(Number.POSITIVE_INFINITY)) );

test();


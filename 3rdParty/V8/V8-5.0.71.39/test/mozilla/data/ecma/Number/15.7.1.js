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

gTestfile = '15.7.1.js';

/**
   File Name:          15.7.1.js
   ECMA Section:       15.7.1 The Number Constructor Called as a Function
   15.7.1.1
   15.7.1.2

   Description:        When Number is called as a function rather than as a
   constructor, it performs a type conversion.
   15.7.1.1    Return a number value (not a Number object)
   computed by ToNumber( value )
   15.7.1.2    Number() returns 0.

   need to add more test cases.  see the gTestcases for
   TypeConversion ToNumber.

   Author:             christine@netscape.com
   Date:               29 september 1997
*/

var SECTION = "15.7.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Number Constructor Called as a Function";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(SECTION, "Number()",                  0,          Number() );
new TestCase(SECTION, "Number(void 0)",            Number.NaN,  Number(void 0) );
new TestCase(SECTION, "Number(null)",              0,          Number(null) );
new TestCase(SECTION, "Number()",                  0,          Number() );
new TestCase(SECTION, "Number(new Number())",      0,          Number( new Number() ) );
new TestCase(SECTION, "Number(0)",                 0,          Number(0) );
new TestCase(SECTION, "Number(1)",                 1,          Number(1) );
new TestCase(SECTION, "Number(-1)",                -1,         Number(-1) );
new TestCase(SECTION, "Number(NaN)",               Number.NaN, Number( Number.NaN ) );
new TestCase(SECTION, "Number('string')",          Number.NaN, Number( "string") );
new TestCase(SECTION, "Number(new String())",      0,          Number( new String() ) );
new TestCase(SECTION, "Number('')",                0,          Number( "" ) );
new TestCase(SECTION, "Number(Infinity)",          Number.POSITIVE_INFINITY,   Number("Infinity") );

new TestCase(SECTION, "Number(new MyObject(100))",  100,        Number(new MyObject(100)) );

test();

function MyObject( value ) {
  this.value = value;
  this.valueOf = new Function( "return this.value" );
}

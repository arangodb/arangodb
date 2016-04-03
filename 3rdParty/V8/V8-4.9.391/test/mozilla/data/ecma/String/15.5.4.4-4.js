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

gTestfile = '15.5.4.4-4.js';

/**
   File Name:          15.5.4.4-4.js
   ECMA Section:       15.5.4.4 String.prototype.charAt(pos)
   Description:        Returns a string containing the character at position
   pos in the string.  If there is no character at that
   string, the result is the empty string.  The result is
   a string value, not a String object.

   When the charAt method is called with one argument,
   pos, the following steps are taken:
   1. Call ToString, with this value as its argument
   2. Call ToInteger pos
   3. Compute the number of characters  in Result(1)
   4. If Result(2) is less than 0 is or not less than
   Result(3), return the empty string
   5. Return a string of length 1 containing one character
   from result (1), the character at position Result(2).

   Note that the charAt function is intentionally generic;
   it does not require that its this value be a String
   object.  Therefore it can be transferred to other kinds
   of objects for use as a method.

   This tests assiging charAt to primitive types..

   Author:             christine@netscape.com
   Date:               2 october 1997
*/
var SECTION = "15.5.4.4-4";
var VERSION = "ECMA_2";
startTest();
var TITLE   = "String.prototype.charAt";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(0)",            "1",     eval("x=new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(0)") );
new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(1)",            ",",     eval("x=new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(1)") );
new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(2)",            "2",     eval("x=new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(2)") );
new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(3)",            ",",     eval("x=new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(3)") );
new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(4)",            "3",     eval("x=new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(4)") );

new TestCase( SECTION,  "x = new Array(); x.charAt = String.prototype.charAt; x.charAt(0)",                    "",      eval("x = new Array(); x.charAt = String.prototype.charAt; x.charAt(0)") );

new TestCase( SECTION,     "x = new Number(123); x.charAt = String.prototype.charAt; x.charAt(0)",            "1",     eval("x=new Number(123); x.charAt = String.prototype.charAt; x.charAt(0)") );
new TestCase( SECTION,     "x = new Number(123); x.charAt = String.prototype.charAt; x.charAt(1)",            "2",     eval("x=new Number(123); x.charAt = String.prototype.charAt; x.charAt(1)") );
new TestCase( SECTION,     "x = new Number(123); x.charAt = String.prototype.charAt; x.charAt(2)",            "3",     eval("x=new Number(123); x.charAt = String.prototype.charAt; x.charAt(2)") );

new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(0)",            "[",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(0)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(1)",            "o",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(1)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(2)",            "b",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(2)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(3)",            "j",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(3)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(4)",            "e",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(4)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(5)",            "c",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(5)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(6)",            "t",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(6)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(7)",            " ",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(7)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(8)",            "O",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(8)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(9)",            "b",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(9)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(10)",            "j",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(10)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(11)",            "e",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(11)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(12)",            "c",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(12)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(13)",            "t",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(13)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(14)",            "]",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(14)") );

new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(0)",            "[",    eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(0)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(1)",            "o",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(1)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(2)",            "b",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(2)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(3)",            "j",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(3)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(4)",            "e",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(4)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(5)",            "c",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(5)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(6)",            "t",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(6)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(7)",            " ",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(7)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(8)",            "F",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(8)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(9)",            "u",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(9)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(10)",            "n",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(10)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(11)",            "c",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(11)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(12)",            "t",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(12)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(13)",            "i",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(13)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(14)",            "o",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(14)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(15)",            "n",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(15)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(16)",            "]",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(16)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(17)",            "",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(17)") );


test();

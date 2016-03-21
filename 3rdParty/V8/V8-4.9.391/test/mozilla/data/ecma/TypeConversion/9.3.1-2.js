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

gTestfile = '9.3.1-2.js';

/**
   File Name:          9.3.1-2.js
   ECMA Section:       9.3  Type Conversion:  ToNumber
   Description:        rules for converting an argument to a number.
   see 9.3.1 for cases for converting strings to numbers.
   special cases:
   undefined           NaN
   Null                NaN
   Boolean             1 if true; +0 if false
   Number              the argument ( no conversion )
   String              see test 9.3.1
   Object              see test 9.3-1

   This tests special cases of ToNumber(string) that are
   not covered in 9.3.1-1.js.

   Author:             christine@netscape.com
   Date:               10 july 1997

*/
var SECTION = "9.3.1-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "ToNumber applied to the String type";

writeHeaderToLog( SECTION + " "+ TITLE);

// A StringNumericLiteral may not use octal notation

new TestCase( SECTION,  "Number(00)",        0,         Number("00"));
new TestCase( SECTION,  "Number(01)",        1,         Number("01"));
new TestCase( SECTION,  "Number(02)",        2,         Number("02"));
new TestCase( SECTION,  "Number(03)",        3,         Number("03"));
new TestCase( SECTION,  "Number(04)",        4,         Number("04"));
new TestCase( SECTION,  "Number(05)",        5,         Number("05"));
new TestCase( SECTION,  "Number(06)",        6,         Number("06"));
new TestCase( SECTION,  "Number(07)",        7,         Number("07"));
new TestCase( SECTION,  "Number(010)",       10,        Number("010"));
new TestCase( SECTION,  "Number(011)",       11,        Number("011"));

// A StringNumericLIteral may have any number of leading 0 digits

new TestCase( SECTION,  "Number(001)",        1,         Number("001"));
new TestCase( SECTION,  "Number(0001)",       1,         Number("0001"));

test();


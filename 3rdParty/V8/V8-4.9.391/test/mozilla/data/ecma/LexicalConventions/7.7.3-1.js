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

gTestfile = '7.7.3-1.js';

/**
   File Name:          7.7.3-1.js
   ECMA Section:       7.7.3 Numeric Literals

   Description:        A numeric literal stands for a value of the Number type
   This value is determined in two steps:  first a
   mathematical value (MV) is derived from the literal;
   second, this mathematical value is rounded, ideally
   using IEEE 754 round-to-nearest mode, to a reprentable
   value of of the number type.

   These test cases came from Waldemar.

   Author:             christine@netscape.com
   Date:               12 June 1998
*/

var SECTION = "7.7.3-1";
var VERSION = "ECMA_1";
var TITLE   = "Numeric Literals";
var BUGNUMBER="122877";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "0x12345678",
	      305419896,
	      0x12345678 );

new TestCase( SECTION,
	      "0x80000000",
	      2147483648,
	      0x80000000 );

new TestCase( SECTION,
	      "0xffffffff",
	      4294967295,
	      0xffffffff );

new TestCase( SECTION,
	      "0x100000000",
	      4294967296,
	      0x100000000 );

new TestCase( SECTION,
	      "077777777777777777",
	      2251799813685247,
	      077777777777777777 );

new TestCase( SECTION,
	      "077777777777777776",
	      2251799813685246,
	      077777777777777776 );

new TestCase( SECTION,
	      "0x1fffffffffffff",
	      9007199254740991,
	      0x1fffffffffffff );

new TestCase( SECTION,
	      "0x20000000000000",
	      9007199254740992,
	      0x20000000000000 );

new TestCase( SECTION,
	      "0x20123456789abc",
	      9027215253084860,
	      0x20123456789abc );

new TestCase( SECTION,
	      "0x20123456789abd",
	      9027215253084860,
	      0x20123456789abd );

new TestCase( SECTION,
	      "0x20123456789abe",
	      9027215253084862,
	      0x20123456789abe );

new TestCase( SECTION,
	      "0x20123456789abf",
	      9027215253084864,
	      0x20123456789abf );

new TestCase( SECTION,
	      "0x1000000000000080",
	      1152921504606847000,
	      0x1000000000000080 );

new TestCase( SECTION,
	      "0x1000000000000081",
	      1152921504606847200,
	      0x1000000000000081 );

new TestCase( SECTION,
	      "0x1000000000000100",
	      1152921504606847200,
	      0x1000000000000100 );

new TestCase( SECTION,
	      "0x100000000000017f",
	      1152921504606847200,
	      0x100000000000017f );

new TestCase( SECTION,
	      "0x1000000000000180",
	      1152921504606847500,
	      0x1000000000000180 );

new TestCase( SECTION,
	      "0x1000000000000181",
	      1152921504606847500,
	      0x1000000000000181 );

new TestCase( SECTION,
	      "0x10000000000001f0",
	      1152921504606847500,
	      0x10000000000001f0 );

new TestCase( SECTION,
	      "0x1000000000000200",
	      1152921504606847500,
	      0x1000000000000200 );

new TestCase( SECTION,
	      "0x100000000000027f",
	      1152921504606847500,
	      0x100000000000027f );

new TestCase( SECTION,
	      "0x1000000000000280",
	      1152921504606847500,
	      0x1000000000000280 );

new TestCase( SECTION,
	      "0x1000000000000281",
	      1152921504606847700,
	      0x1000000000000281 );

new TestCase( SECTION,
	      "0x10000000000002ff",
	      1152921504606847700,
	      0x10000000000002ff );

new TestCase( SECTION,
	      "0x1000000000000300",
	      1152921504606847700,
	      0x1000000000000300 );

new TestCase( SECTION,
	      "0x10000000000000000",
	      18446744073709552000,
	      0x10000000000000000 );

test();


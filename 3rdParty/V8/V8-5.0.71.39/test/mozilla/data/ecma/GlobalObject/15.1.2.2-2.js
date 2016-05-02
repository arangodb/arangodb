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

gTestfile = '15.1.2.2-2.js';

/**
   File Name:          15.1.2.2-1.js
   ECMA Section:       15.1.2.2 Function properties of the global object
   parseInt( string, radix )

   Description:        parseInt test cases written by waldemar, and documented in
   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=123874.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.1.2.2-2";
var VERSION = "ECMA_1";
var TITLE   = "parseInt(string, radix)";
var BUGNUMBER = "none";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      'parseInt("000000100000000100100011010001010110011110001001101010111100",2)',
	      9027215253084860,
	      parseInt("000000100000000100100011010001010110011110001001101010111100",2) );

new TestCase( SECTION,
	      'parseInt("000000100000000100100011010001010110011110001001101010111101",2)',
	      9027215253084860,
	      parseInt("000000100000000100100011010001010110011110001001101010111101",2));

new TestCase( SECTION,
	      'parseInt("000000100000000100100011010001010110011110001001101010111111",2)',
	      9027215253084864,
	      parseInt("000000100000000100100011010001010110011110001001101010111111",2) );

new TestCase( SECTION,
	      'parseInt("0000001000000001001000110100010101100111100010011010101111010",2)',
	      18054430506169720,
	      parseInt("0000001000000001001000110100010101100111100010011010101111010",2) );

new TestCase( SECTION,
	      'parseInt("0000001000000001001000110100010101100111100010011010101111011",2)',
	      18054430506169724,
	      parseInt("0000001000000001001000110100010101100111100010011010101111011",2));

new TestCase( SECTION,
	      'parseInt("0000001000000001001000110100010101100111100010011010101111100",2)',
	      18054430506169724,
	      parseInt("0000001000000001001000110100010101100111100010011010101111100",2) );

new TestCase( SECTION,
	      'parseInt("0000001000000001001000110100010101100111100010011010101111110",2)',
	      18054430506169728,
	      parseInt("0000001000000001001000110100010101100111100010011010101111110",2) );

new TestCase( SECTION,
	      'parseInt("yz",35)',
	      34,
	      parseInt("yz",35) );

new TestCase( SECTION,
	      'parseInt("yz",36)',
	      1259,
	      parseInt("yz",36) );

new TestCase( SECTION,
	      'parseInt("yz",37)',
	      NaN,
	      parseInt("yz",37) );

new TestCase( SECTION,
	      'parseInt("+77")',
	      77,
	      parseInt("+77") );

new TestCase( SECTION,
	      'parseInt("-77",9)',
	      -70,
	      parseInt("-77",9) );

new TestCase( SECTION,
	      'parseInt("\u20001234\u2000")',
	      1234,
	      parseInt("\u20001234\u2000") );

new TestCase( SECTION,
	      'parseInt("123456789012345678")',
	      123456789012345680,
	      parseInt("123456789012345678") );

new TestCase( SECTION,
	      'parseInt("9",8)',
	      NaN,
	      parseInt("9",8) );

new TestCase( SECTION,
	      'parseInt("1e2")',
	      1,
	      parseInt("1e2") );

new TestCase( SECTION,
	      'parseInt("1.9999999999999999999")',
	      1,
	      parseInt("1.9999999999999999999") );

new TestCase( SECTION,
	      'parseInt("0x10")',
	      16,
	      parseInt("0x10") );

new TestCase( SECTION,
	      'parseInt("0x10",10)',
	      0,
	      parseInt("0x10",10));

new TestCase( SECTION,
	      'parseInt("0022")',
	      18,
	      parseInt("0022"));

new TestCase( SECTION,
	      'parseInt("0022",10)',
	      22,
	      parseInt("0022",10) );

new TestCase( SECTION,
	      'parseInt("0x1000000000000080")',
	      1152921504606847000,
	      parseInt("0x1000000000000080") );

new TestCase( SECTION,
	      'parseInt("0x1000000000000081")',
	      1152921504606847200,
	      parseInt("0x1000000000000081") );

s =
  "0xFFFFFFFFFFFFF80000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"

  s += "0000000000000000000000000000000000000";

new TestCase( SECTION,
	      "s = " + s +"; -s",
	      -1.7976931348623157e+308,
	      -s );

s =
  "0xFFFFFFFFFFFFF80000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
s += "0000000000000000000000000000000000001";

new TestCase( SECTION,
	      "s = " + s +"; -s",
	      -1.7976931348623157e+308,
	      -s );


s = "0xFFFFFFFFFFFFFC0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

s += "0000000000000000000000000000000000000"


new TestCase( SECTION,
	      "s = " + s + "; -s",
	      -Infinity,
	      -s );

s = "0xFFFFFFFFFFFFFB0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
s += "0000000000000000000000000000000000001";

new TestCase( SECTION,
	      "s = " + s + "; -s",
	      -1.7976931348623157e+308,
	      -s );

s += "0"

new TestCase( SECTION,
	      "s = " + s + "; -s",
	      -Infinity,
	      -s );

new TestCase( SECTION,
	      'parseInt(s)',
	      Infinity,
	      parseInt(s) );

new TestCase( SECTION,
	      'parseInt(s,32)',
	      0,
	      parseInt(s,32) );

new TestCase( SECTION,
	      'parseInt(s,36)',
	      Infinity,
	      parseInt(s,36));

test();


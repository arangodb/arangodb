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

gTestfile = '7.7.3.js';

/**
   File Name:          7.7.3.js
   ECMA Section:       7.7.3 Numeric Literals

   Description:        A numeric literal stands for a value of the Number type
   This value is determined in two steps:  first a
   mathematical value (MV) is derived from the literal;
   second, this mathematical value is rounded, ideally
   using IEEE 754 round-to-nearest mode, to a reprentable
   value of of the number type.

   Author:             christine@netscape.com
   Date:               16 september 1997
*/
var SECTION = "7.7.3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Numeric Literals";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, "0",     0,      0 );
new TestCase( SECTION, "1",     1,      1 );
new TestCase( SECTION, "2",     2,      2 );
new TestCase( SECTION, "3",     3,      3 );
new TestCase( SECTION, "4",     4,      4 );
new TestCase( SECTION, "5",     5,      5 );
new TestCase( SECTION, "6",     6,      6 );
new TestCase( SECTION, "7",     7,      7 );
new TestCase( SECTION, "8",     8,      8 );
new TestCase( SECTION, "9",     9,      9 );

new TestCase( SECTION, "0.",     0,      0. );
new TestCase( SECTION, "1.",     1,      1. );
new TestCase( SECTION, "2.",     2,      2. );
new TestCase( SECTION, "3.",     3,      3. );
new TestCase( SECTION, "4.",     4,      4. );

new TestCase( SECTION, "0.e0",  0,      0.e0 );
new TestCase( SECTION, "1.e1",  10,     1.e1 );
new TestCase( SECTION, "2.e2",  200,    2.e2 );
new TestCase( SECTION, "3.e3",  3000,   3.e3 );
new TestCase( SECTION, "4.e4",  40000,  4.e4 );

new TestCase( SECTION, "0.1e0",  .1,    0.1e0 );
new TestCase( SECTION, "1.1e1",  11,    1.1e1 );
new TestCase( SECTION, "2.2e2",  220,   2.2e2 );
new TestCase( SECTION, "3.3e3",  3300,  3.3e3 );
new TestCase( SECTION, "4.4e4",  44000, 4.4e4 );

new TestCase( SECTION, ".1e0",  .1,   .1e0 );
new TestCase( SECTION, ".1e1",  1,    .1e1 );
new TestCase( SECTION, ".2e2",  20,   .2e2 );
new TestCase( SECTION, ".3e3",  300,  .3e3 );
new TestCase( SECTION, ".4e4",  4000, .4e4 );

new TestCase( SECTION, "0e0",  0,     0e0 );
new TestCase( SECTION, "1e1",  10,    1e1 );
new TestCase( SECTION, "2e2",  200,   2e2 );
new TestCase( SECTION, "3e3",  3000,  3e3 );
new TestCase( SECTION, "4e4",  40000, 4e4 );

new TestCase( SECTION, "0e0",  0,     0e0 );
new TestCase( SECTION, "1e1",  10,    1e1 );
new TestCase( SECTION, "2e2",  200,   2e2 );
new TestCase( SECTION, "3e3",  3000,  3e3 );
new TestCase( SECTION, "4e4",  40000, 4e4 );

new TestCase( SECTION, "0E0",  0,     0E0 );
new TestCase( SECTION, "1E1",  10,    1E1 );
new TestCase( SECTION, "2E2",  200,   2E2 );
new TestCase( SECTION, "3E3",  3000,  3E3 );
new TestCase( SECTION, "4E4",  40000, 4E4 );

new TestCase( SECTION, "1.e-1",  0.1,     1.e-1 );
new TestCase( SECTION, "2.e-2",  0.02,    2.e-2 );
new TestCase( SECTION, "3.e-3",  0.003,   3.e-3 );
new TestCase( SECTION, "4.e-4",  0.0004,  4.e-4 );

new TestCase( SECTION, "0.1e-0",  .1,     0.1e-0 );
new TestCase( SECTION, "1.1e-1",  0.11,   1.1e-1 );
new TestCase( SECTION, "2.2e-2",  .022,   2.2e-2 );
new TestCase( SECTION, "3.3e-3",  .0033,  3.3e-3 );
new TestCase( SECTION, "4.4e-4",  .00044, 4.4e-4 );

new TestCase( SECTION, ".1e-0",  .1,    .1e-0 );
new TestCase( SECTION, ".1e-1",  .01,    .1e-1 );
new TestCase( SECTION, ".2e-2",  .002,   .2e-2 );
new TestCase( SECTION, ".3e-3",  .0003,  .3e-3 );
new TestCase( SECTION, ".4e-4",  .00004, .4e-4 );

new TestCase( SECTION, "1.e+1",  10,     1.e+1 );
new TestCase( SECTION, "2.e+2",  200,    2.e+2 );
new TestCase( SECTION, "3.e+3",  3000,   3.e+3 );
new TestCase( SECTION, "4.e+4",  40000,  4.e+4 );

new TestCase( SECTION, "0.1e+0",  .1,    0.1e+0 );
new TestCase( SECTION, "1.1e+1",  11,    1.1e+1 );
new TestCase( SECTION, "2.2e+2",  220,   2.2e+2 );
new TestCase( SECTION, "3.3e+3",  3300,  3.3e+3 );
new TestCase( SECTION, "4.4e+4",  44000, 4.4e+4 );

new TestCase( SECTION, ".1e+0",  .1,   .1e+0 );
new TestCase( SECTION, ".1e+1",  1,    .1e+1 );
new TestCase( SECTION, ".2e+2",  20,   .2e+2 );
new TestCase( SECTION, ".3e+3",  300,  .3e+3 );
new TestCase( SECTION, ".4e+4",  4000, .4e+4 );

new TestCase( SECTION, "0x0",  0,   0x0 );
new TestCase( SECTION, "0x1",  1,   0x1 );
new TestCase( SECTION, "0x2",  2,   0x2 );
new TestCase( SECTION, "0x3",  3,   0x3 );
new TestCase( SECTION, "0x4",  4,   0x4 );
new TestCase( SECTION, "0x5",  5,   0x5 );
new TestCase( SECTION, "0x6",  6,   0x6 );
new TestCase( SECTION, "0x7",  7,   0x7 );
new TestCase( SECTION, "0x8",  8,   0x8 );
new TestCase( SECTION, "0x9",  9,   0x9 );
new TestCase( SECTION, "0xa",  10,  0xa );
new TestCase( SECTION, "0xb",  11,  0xb );
new TestCase( SECTION, "0xc",  12,  0xc );
new TestCase( SECTION, "0xd",  13,  0xd );
new TestCase( SECTION, "0xe",  14,  0xe );
new TestCase( SECTION, "0xf",  15,  0xf );

new TestCase( SECTION, "0X0",  0,   0X0 );
new TestCase( SECTION, "0X1",  1,   0X1 );
new TestCase( SECTION, "0X2",  2,   0X2 );
new TestCase( SECTION, "0X3",  3,   0X3 );
new TestCase( SECTION, "0X4",  4,   0X4 );
new TestCase( SECTION, "0X5",  5,   0X5 );
new TestCase( SECTION, "0X6",  6,   0X6 );
new TestCase( SECTION, "0X7",  7,   0X7 );
new TestCase( SECTION, "0X8",  8,   0X8 );
new TestCase( SECTION, "0X9",  9,   0X9 );
new TestCase( SECTION, "0Xa",  10,  0Xa );
new TestCase( SECTION, "0Xb",  11,  0Xb );
new TestCase( SECTION, "0Xc",  12,  0Xc );
new TestCase( SECTION, "0Xd",  13,  0Xd );
new TestCase( SECTION, "0Xe",  14,  0Xe );
new TestCase( SECTION, "0Xf",  15,  0Xf );

new TestCase( SECTION, "0x0",  0,   0x0 );
new TestCase( SECTION, "0x1",  1,   0x1 );
new TestCase( SECTION, "0x2",  2,   0x2 );
new TestCase( SECTION, "0x3",  3,   0x3 );
new TestCase( SECTION, "0x4",  4,   0x4 );
new TestCase( SECTION, "0x5",  5,   0x5 );
new TestCase( SECTION, "0x6",  6,   0x6 );
new TestCase( SECTION, "0x7",  7,   0x7 );
new TestCase( SECTION, "0x8",  8,   0x8 );
new TestCase( SECTION, "0x9",  9,   0x9 );
new TestCase( SECTION, "0xA",  10,  0xA );
new TestCase( SECTION, "0xB",  11,  0xB );
new TestCase( SECTION, "0xC",  12,  0xC );
new TestCase( SECTION, "0xD",  13,  0xD );
new TestCase( SECTION, "0xE",  14,  0xE );
new TestCase( SECTION, "0xF",  15,  0xF );

new TestCase( SECTION, "0X0",  0,   0X0 );
new TestCase( SECTION, "0X1",  1,   0X1 );
new TestCase( SECTION, "0X2",  2,   0X2 );
new TestCase( SECTION, "0X3",  3,   0X3 );
new TestCase( SECTION, "0X4",  4,   0X4 );
new TestCase( SECTION, "0X5",  5,   0X5 );
new TestCase( SECTION, "0X6",  6,   0X6 );
new TestCase( SECTION, "0X7",  7,   0X7 );
new TestCase( SECTION, "0X8",  8,   0X8 );
new TestCase( SECTION, "0X9",  9,   0X9 );
new TestCase( SECTION, "0XA",  10,  0XA );
new TestCase( SECTION, "0XB",  11,  0XB );
new TestCase( SECTION, "0XC",  12,  0XC );
new TestCase( SECTION, "0XD",  13,  0XD );
new TestCase( SECTION, "0XE",  14,  0XE );
new TestCase( SECTION, "0XF",  15,  0XF );


new TestCase( SECTION, "00",  0,   00 );
new TestCase( SECTION, "01",  1,   01 );
new TestCase( SECTION, "02",  2,   02 );
new TestCase( SECTION, "03",  3,   03 );
new TestCase( SECTION, "04",  4,   04 );
new TestCase( SECTION, "05",  5,   05 );
new TestCase( SECTION, "06",  6,   06 );
new TestCase( SECTION, "07",  7,   07 );

new TestCase( SECTION, "000",  0,   000 );
new TestCase( SECTION, "011",  9,   011 );
new TestCase( SECTION, "022",  18,  022 );
new TestCase( SECTION, "033",  27,  033 );
new TestCase( SECTION, "044",  36,  044 );
new TestCase( SECTION, "055",  45,  055 );
new TestCase( SECTION, "066",  54,  066 );
new TestCase( SECTION, "077",  63,   077 );

new TestCase( SECTION, "0.00000000001",  0.00000000001,  0.00000000001 );
new TestCase( SECTION, "0.00000000001e-2",  0.0000000000001,  0.00000000001e-2 );


new TestCase( SECTION,
	      "123456789012345671.9999",
	      "123456789012345660",
	      123456789012345671.9999 +"");
new TestCase( SECTION,
	      "123456789012345672",
	      "123456789012345660",
	      123456789012345672 +"");

new TestCase(   SECTION,
		"123456789012345672.000000000000000000000000000",
		"123456789012345660",
		123456789012345672.000000000000000000000000000 +"");

new TestCase( SECTION,
	      "123456789012345672.01",
	      "123456789012345680",
	      123456789012345672.01 +"");

new TestCase( SECTION,
	      "123456789012345672.000000000000000000000000001+'' == 123456789012345680 || 123456789012345660",
	      true,
	      ( 123456789012345672.00000000000000000000000000 +""  == 1234567890 * 100000000 + 12345680 )
	      ||
	      ( 123456789012345672.00000000000000000000000000 +""  == 1234567890 * 100000000 + 12345660) );

new TestCase( SECTION,
	      "123456789012345673",
	      "123456789012345680",
	      123456789012345673 +"" );

new TestCase( SECTION,
	      "-123456789012345671.9999",
	      "-123456789012345660",
	      -123456789012345671.9999 +"" );

new TestCase( SECTION,
	      "-123456789012345672",
	      "-123456789012345660",
	      -123456789012345672+"");

new TestCase( SECTION,
	      "-123456789012345672.000000000000000000000000000",
	      "-123456789012345660",
	      -123456789012345672.000000000000000000000000000 +"");

new TestCase( SECTION,
	      "-123456789012345672.01",
	      "-123456789012345680",
	      -123456789012345672.01 +"" );

new TestCase( SECTION,
	      "-123456789012345672.000000000000000000000000001 == -123456789012345680 or -123456789012345660",
	      true,
	      (-123456789012345672.000000000000000000000000001 +"" == -1234567890 * 100000000 -12345680)
	      ||
	      (-123456789012345672.000000000000000000000000001 +"" == -1234567890 * 100000000 -12345660));

new TestCase( SECTION,
	      -123456789012345673,
	      "-123456789012345680",
	      -123456789012345673 +"");

new TestCase( SECTION,
	      "12345678901234567890",
	      "12345678901234567000",
	      12345678901234567890 +"" );


/*
  new TestCase( SECTION, "12345678901234567",         "12345678901234567",        12345678901234567+"" );
  new TestCase( SECTION, "123456789012345678",        "123456789012345678",       123456789012345678+"" );
  new TestCase( SECTION, "1234567890123456789",       "1234567890123456789",      1234567890123456789+"" );
  new TestCase( SECTION, "12345678901234567890",      "12345678901234567890",     12345678901234567890+"" );
  new TestCase( SECTION, "123456789012345678900",     "123456789012345678900",    123456789012345678900+"" );
  new TestCase( SECTION, "1234567890123456789000",    "1234567890123456789000",   1234567890123456789000+"" );
*/
new TestCase( SECTION, "0x1",          1,          0x1 );
new TestCase( SECTION, "0x10",         16,         0x10 );
new TestCase( SECTION, "0x100",        256,        0x100 );
new TestCase( SECTION, "0x1000",       4096,       0x1000 );
new TestCase( SECTION, "0x10000",      65536,      0x10000 );
new TestCase( SECTION, "0x100000",     1048576,    0x100000 );
new TestCase( SECTION, "0x1000000",    16777216,   0x1000000 );
new TestCase( SECTION, "0x10000000",   268435456,  0x10000000 );
/*
  new TestCase( SECTION, "0x100000000",          4294967296,      0x100000000 );
  new TestCase( SECTION, "0x1000000000",         68719476736,     0x1000000000 );
  new TestCase( SECTION, "0x10000000000",        1099511627776,     0x10000000000 );
*/

test();

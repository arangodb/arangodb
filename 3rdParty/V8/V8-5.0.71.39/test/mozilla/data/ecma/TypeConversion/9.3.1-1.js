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

gTestfile = '9.3.1-1.js';

/**
   File Name:          9.3.1-1.js
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


   This tests ToNumber applied to the string type

   Author:             christine@netscape.com
   Date:               10 july 1997

*/
var SECTION = "9.3.1-1";
var VERSION = "ECMA_1";
var TITLE   = "ToNumber applied to the String type";
var BUGNUMBER="77391";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);


//  StringNumericLiteral:::StrWhiteSpace:::StrWhiteSpaceChar StrWhiteSpace:::
//
//  Name    Unicode Value   Escape Sequence
//  <TAB>   0X0009          \t
//  <SP>    0X0020
//  <FF>    0X000C          \f
//  <VT>    0X000B
//  <CR>    0X000D          \r
//  <LF>    0X000A          \n
new TestCase( SECTION,  "Number('')",           0,      Number("") );
new TestCase( SECTION,  "Number(' ')",          0,      Number(" ") );
new TestCase( SECTION,  "Number(\\t)",          0,      Number("\t") );
new TestCase( SECTION,  "Number(\\n)",          0,      Number("\n") );
new TestCase( SECTION,  "Number(\\r)",          0,      Number("\r") );
new TestCase( SECTION,  "Number(\\f)",          0,      Number("\f") );

new TestCase( SECTION,  "Number(String.fromCharCode(0x0009)",   0,  Number(String.fromCharCode(0x0009)) );
new TestCase( SECTION,  "Number(String.fromCharCode(0x0020)",   0,  Number(String.fromCharCode(0x0020)) );
new TestCase( SECTION,  "Number(String.fromCharCode(0x000C)",   0,  Number(String.fromCharCode(0x000C)) );
new TestCase( SECTION,  "Number(String.fromCharCode(0x000B)",   0,  Number(String.fromCharCode(0x000B)) );
new TestCase( SECTION,  "Number(String.fromCharCode(0x000D)",   0,  Number(String.fromCharCode(0x000D)) );
new TestCase( SECTION,  "Number(String.fromCharCode(0x000A)",   0,  Number(String.fromCharCode(0x000A)) );

//  a StringNumericLiteral may be preceeded or followed by whitespace and/or
//  line terminators

new TestCase( SECTION,  "Number( '   ' +  999 )",        999,    Number( '   '+999) );
new TestCase( SECTION,  "Number( '\\n'  + 999 )",       999,    Number( '\n' +999) );
new TestCase( SECTION,  "Number( '\\r'  + 999 )",       999,    Number( '\r' +999) );
new TestCase( SECTION,  "Number( '\\t'  + 999 )",       999,    Number( '\t' +999) );
new TestCase( SECTION,  "Number( '\\f'  + 999 )",       999,    Number( '\f' +999) );

new TestCase( SECTION,  "Number( 999 + '   ' )",        999,    Number( 999+'   ') );
new TestCase( SECTION,  "Number( 999 + '\\n' )",        999,    Number( 999+'\n' ) );
new TestCase( SECTION,  "Number( 999 + '\\r' )",        999,    Number( 999+'\r' ) );
new TestCase( SECTION,  "Number( 999 + '\\t' )",        999,    Number( 999+'\t' ) );
new TestCase( SECTION,  "Number( 999 + '\\f' )",        999,    Number( 999+'\f' ) );

new TestCase( SECTION,  "Number( '\\n'  + 999 + '\\n' )",         999,    Number( '\n' +999+'\n' ) );
new TestCase( SECTION,  "Number( '\\r'  + 999 + '\\r' )",         999,    Number( '\r' +999+'\r' ) );
new TestCase( SECTION,  "Number( '\\t'  + 999 + '\\t' )",         999,    Number( '\t' +999+'\t' ) );
new TestCase( SECTION,  "Number( '\\f'  + 999 + '\\f' )",         999,    Number( '\f' +999+'\f' ) );

new TestCase( SECTION,  "Number( '   ' +  '999' )",     999,    Number( '   '+'999') );
new TestCase( SECTION,  "Number( '\\n'  + '999' )",       999,    Number( '\n' +'999') );
new TestCase( SECTION,  "Number( '\\r'  + '999' )",       999,    Number( '\r' +'999') );
new TestCase( SECTION,  "Number( '\\t'  + '999' )",       999,    Number( '\t' +'999') );
new TestCase( SECTION,  "Number( '\\f'  + '999' )",       999,    Number( '\f' +'999') );

new TestCase( SECTION,  "Number( '999' + '   ' )",        999,    Number( '999'+'   ') );
new TestCase( SECTION,  "Number( '999' + '\\n' )",        999,    Number( '999'+'\n' ) );
new TestCase( SECTION,  "Number( '999' + '\\r' )",        999,    Number( '999'+'\r' ) );
new TestCase( SECTION,  "Number( '999' + '\\t' )",        999,    Number( '999'+'\t' ) );
new TestCase( SECTION,  "Number( '999' + '\\f' )",        999,    Number( '999'+'\f' ) );

new TestCase( SECTION,  "Number( '\\n'  + '999' + '\\n' )",         999,    Number( '\n' +'999'+'\n' ) );
new TestCase( SECTION,  "Number( '\\r'  + '999' + '\\r' )",         999,    Number( '\r' +'999'+'\r' ) );
new TestCase( SECTION,  "Number( '\\t'  + '999' + '\\t' )",         999,    Number( '\t' +'999'+'\t' ) );
new TestCase( SECTION,  "Number( '\\f'  + '999' + '\\f' )",         999,    Number( '\f' +'999'+'\f' ) );

new TestCase( SECTION,  "Number( String.fromCharCode(0x0009) +  '99' )",    99,     Number( String.fromCharCode(0x0009) +  '99' ) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x0020) +  '99' )",    99,     Number( String.fromCharCode(0x0020) +  '99' ) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000C) +  '99' )",    99,     Number( String.fromCharCode(0x000C) +  '99' ) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000B) +  '99' )",    99,     Number( String.fromCharCode(0x000B) +  '99' ) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000D) +  '99' )",    99,     Number( String.fromCharCode(0x000D) +  '99' ) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000A) +  '99' )",    99,     Number( String.fromCharCode(0x000A) +  '99' ) );

new TestCase( SECTION,  "Number( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x0009)",    99,     Number( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x0009)) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x0020) +  '99' + String.fromCharCode(0x0020)",    99,     Number( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x0020)) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000C) +  '99' + String.fromCharCode(0x000C)",    99,     Number( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x000C)) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000D) +  '99' + String.fromCharCode(0x000D)",    99,     Number( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x000D)) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000B) +  '99' + String.fromCharCode(0x000B)",    99,     Number( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x000B)) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000A) +  '99' + String.fromCharCode(0x000A)",    99,     Number( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x000A)) );

new TestCase( SECTION,  "Number( '99' + String.fromCharCode(0x0009)",    99,     Number( '99' + String.fromCharCode(0x0009)) );
new TestCase( SECTION,  "Number( '99' + String.fromCharCode(0x0020)",    99,     Number( '99' + String.fromCharCode(0x0020)) );
new TestCase( SECTION,  "Number( '99' + String.fromCharCode(0x000C)",    99,     Number( '99' + String.fromCharCode(0x000C)) );
new TestCase( SECTION,  "Number( '99' + String.fromCharCode(0x000D)",    99,     Number( '99' + String.fromCharCode(0x000D)) );
new TestCase( SECTION,  "Number( '99' + String.fromCharCode(0x000B)",    99,     Number( '99' + String.fromCharCode(0x000B)) );
new TestCase( SECTION,  "Number( '99' + String.fromCharCode(0x000A)",    99,     Number( '99' + String.fromCharCode(0x000A)) );

new TestCase( SECTION,  "Number( String.fromCharCode(0x0009) +  99 )",    99,     Number( String.fromCharCode(0x0009) +  99 ) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x0020) +  99 )",    99,     Number( String.fromCharCode(0x0020) +  99 ) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000C) +  99 )",    99,     Number( String.fromCharCode(0x000C) +  99 ) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000B) +  99 )",    99,     Number( String.fromCharCode(0x000B) +  99 ) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000D) +  99 )",    99,     Number( String.fromCharCode(0x000D) +  99 ) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000A) +  99 )",    99,     Number( String.fromCharCode(0x000A) +  99 ) );

new TestCase( SECTION,  "Number( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x0009)",    99,     Number( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x0009)) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x0020) +  99 + String.fromCharCode(0x0020)",    99,     Number( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x0020)) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000C) +  99 + String.fromCharCode(0x000C)",    99,     Number( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x000C)) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000D) +  99 + String.fromCharCode(0x000D)",    99,     Number( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x000D)) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000B) +  99 + String.fromCharCode(0x000B)",    99,     Number( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x000B)) );
new TestCase( SECTION,  "Number( String.fromCharCode(0x000A) +  99 + String.fromCharCode(0x000A)",    99,     Number( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x000A)) );

new TestCase( SECTION,  "Number( 99 + String.fromCharCode(0x0009)",    99,     Number( 99 + String.fromCharCode(0x0009)) );
new TestCase( SECTION,  "Number( 99 + String.fromCharCode(0x0020)",    99,     Number( 99 + String.fromCharCode(0x0020)) );
new TestCase( SECTION,  "Number( 99 + String.fromCharCode(0x000C)",    99,     Number( 99 + String.fromCharCode(0x000C)) );
new TestCase( SECTION,  "Number( 99 + String.fromCharCode(0x000D)",    99,     Number( 99 + String.fromCharCode(0x000D)) );
new TestCase( SECTION,  "Number( 99 + String.fromCharCode(0x000B)",    99,     Number( 99 + String.fromCharCode(0x000B)) );
new TestCase( SECTION,  "Number( 99 + String.fromCharCode(0x000A)",    99,     Number( 99 + String.fromCharCode(0x000A)) );


// StrNumericLiteral:::StrDecimalLiteral:::Infinity

new TestCase( SECTION,  "Number('Infinity')",   Math.pow(10,10000),   Number("Infinity") );
new TestCase( SECTION,  "Number('-Infinity')", -Math.pow(10,10000),   Number("-Infinity") );
new TestCase( SECTION,  "Number('+Infinity')",  Math.pow(10,10000),   Number("+Infinity") );

// StrNumericLiteral:::   StrDecimalLiteral ::: DecimalDigits . DecimalDigits opt ExponentPart opt

new TestCase( SECTION,  "Number('0')",          0,          Number("0") );
new TestCase( SECTION,  "Number('-0')",         -0,         Number("-0") );
new TestCase( SECTION,  "Number('+0')",          0,         Number("+0") );

new TestCase( SECTION,  "Number('1')",          1,          Number("1") );
new TestCase( SECTION,  "Number('-1')",         -1,         Number("-1") );
new TestCase( SECTION,  "Number('+1')",          1,         Number("+1") );

new TestCase( SECTION,  "Number('2')",          2,          Number("2") );
new TestCase( SECTION,  "Number('-2')",         -2,         Number("-2") );
new TestCase( SECTION,  "Number('+2')",          2,         Number("+2") );

new TestCase( SECTION,  "Number('3')",          3,          Number("3") );
new TestCase( SECTION,  "Number('-3')",         -3,         Number("-3") );
new TestCase( SECTION,  "Number('+3')",          3,         Number("+3") );

new TestCase( SECTION,  "Number('4')",          4,          Number("4") );
new TestCase( SECTION,  "Number('-4')",         -4,         Number("-4") );
new TestCase( SECTION,  "Number('+4')",          4,         Number("+4") );

new TestCase( SECTION,  "Number('5')",          5,          Number("5") );
new TestCase( SECTION,  "Number('-5')",         -5,         Number("-5") );
new TestCase( SECTION,  "Number('+5')",          5,         Number("+5") );

new TestCase( SECTION,  "Number('6')",          6,          Number("6") );
new TestCase( SECTION,  "Number('-6')",         -6,         Number("-6") );
new TestCase( SECTION,  "Number('+6')",          6,         Number("+6") );

new TestCase( SECTION,  "Number('7')",          7,          Number("7") );
new TestCase( SECTION,  "Number('-7')",         -7,         Number("-7") );
new TestCase( SECTION,  "Number('+7')",          7,         Number("+7") );

new TestCase( SECTION,  "Number('8')",          8,          Number("8") );
new TestCase( SECTION,  "Number('-8')",         -8,         Number("-8") );
new TestCase( SECTION,  "Number('+8')",          8,         Number("+8") );

new TestCase( SECTION,  "Number('9')",          9,          Number("9") );
new TestCase( SECTION,  "Number('-9')",         -9,         Number("-9") );
new TestCase( SECTION,  "Number('+9')",          9,         Number("+9") );

new TestCase( SECTION,  "Number('3.14159')",    3.14159,    Number("3.14159") );
new TestCase( SECTION,  "Number('-3.14159')",   -3.14159,   Number("-3.14159") );
new TestCase( SECTION,  "Number('+3.14159')",   3.14159,    Number("+3.14159") );

new TestCase( SECTION,  "Number('3.')",         3,          Number("3.") );
new TestCase( SECTION,  "Number('-3.')",        -3,         Number("-3.") );
new TestCase( SECTION,  "Number('+3.')",        3,          Number("+3.") );

new TestCase( SECTION,  "Number('3.e1')",       30,         Number("3.e1") );
new TestCase( SECTION,  "Number('-3.e1')",      -30,        Number("-3.e1") );
new TestCase( SECTION,  "Number('+3.e1')",      30,         Number("+3.e1") );

new TestCase( SECTION,  "Number('3.e+1')",       30,         Number("3.e+1") );
new TestCase( SECTION,  "Number('-3.e+1')",      -30,        Number("-3.e+1") );
new TestCase( SECTION,  "Number('+3.e+1')",      30,         Number("+3.e+1") );

new TestCase( SECTION,  "Number('3.e-1')",       .30,         Number("3.e-1") );
new TestCase( SECTION,  "Number('-3.e-1')",      -.30,        Number("-3.e-1") );
new TestCase( SECTION,  "Number('+3.e-1')",      .30,         Number("+3.e-1") );

// StrDecimalLiteral:::  .DecimalDigits ExponentPart opt

new TestCase( SECTION,  "Number('.00001')",     0.00001,    Number(".00001") );
new TestCase( SECTION,  "Number('+.00001')",    0.00001,    Number("+.00001") );
new TestCase( SECTION,  "Number('-0.0001')",    -0.00001,   Number("-.00001") );

new TestCase( SECTION,  "Number('.01e2')",      1,          Number(".01e2") );
new TestCase( SECTION,  "Number('+.01e2')",     1,          Number("+.01e2") );
new TestCase( SECTION,  "Number('-.01e2')",     -1,         Number("-.01e2") );

new TestCase( SECTION,  "Number('.01e+2')",      1,         Number(".01e+2") );
new TestCase( SECTION,  "Number('+.01e+2')",     1,         Number("+.01e+2") );
new TestCase( SECTION,  "Number('-.01e+2')",     -1,        Number("-.01e+2") );

new TestCase( SECTION,  "Number('.01e-2')",      0.0001,    Number(".01e-2") );
new TestCase( SECTION,  "Number('+.01e-2')",     0.0001,    Number("+.01e-2") );
new TestCase( SECTION,  "Number('-.01e-2')",     -0.0001,   Number("-.01e-2") );

//  StrDecimalLiteral:::    DecimalDigits ExponentPart opt

new TestCase( SECTION,  "Number('1234e5')",     123400000,  Number("1234e5") );
new TestCase( SECTION,  "Number('+1234e5')",    123400000,  Number("+1234e5") );
new TestCase( SECTION,  "Number('-1234e5')",    -123400000, Number("-1234e5") );

new TestCase( SECTION,  "Number('1234e+5')",    123400000,  Number("1234e+5") );
new TestCase( SECTION,  "Number('+1234e+5')",   123400000,  Number("+1234e+5") );
new TestCase( SECTION,  "Number('-1234e+5')",   -123400000, Number("-1234e+5") );

new TestCase( SECTION,  "Number('1234e-5')",     0.01234,  Number("1234e-5") );
new TestCase( SECTION,  "Number('+1234e-5')",    0.01234,  Number("+1234e-5") );
new TestCase( SECTION,  "Number('-1234e-5')",    -0.01234, Number("-1234e-5") );

// StrNumericLiteral::: HexIntegerLiteral

new TestCase( SECTION,  "Number('0x0')",        0,          Number("0x0"));
new TestCase( SECTION,  "Number('0x1')",        1,          Number("0x1"));
new TestCase( SECTION,  "Number('0x2')",        2,          Number("0x2"));
new TestCase( SECTION,  "Number('0x3')",        3,          Number("0x3"));
new TestCase( SECTION,  "Number('0x4')",        4,          Number("0x4"));
new TestCase( SECTION,  "Number('0x5')",        5,          Number("0x5"));
new TestCase( SECTION,  "Number('0x6')",        6,          Number("0x6"));
new TestCase( SECTION,  "Number('0x7')",        7,          Number("0x7"));
new TestCase( SECTION,  "Number('0x8')",        8,          Number("0x8"));
new TestCase( SECTION,  "Number('0x9')",        9,          Number("0x9"));
new TestCase( SECTION,  "Number('0xa')",        10,         Number("0xa"));
new TestCase( SECTION,  "Number('0xb')",        11,         Number("0xb"));
new TestCase( SECTION,  "Number('0xc')",        12,         Number("0xc"));
new TestCase( SECTION,  "Number('0xd')",        13,         Number("0xd"));
new TestCase( SECTION,  "Number('0xe')",        14,         Number("0xe"));
new TestCase( SECTION,  "Number('0xf')",        15,         Number("0xf"));
new TestCase( SECTION,  "Number('0xA')",        10,         Number("0xA"));
new TestCase( SECTION,  "Number('0xB')",        11,         Number("0xB"));
new TestCase( SECTION,  "Number('0xC')",        12,         Number("0xC"));
new TestCase( SECTION,  "Number('0xD')",        13,         Number("0xD"));
new TestCase( SECTION,  "Number('0xE')",        14,         Number("0xE"));
new TestCase( SECTION,  "Number('0xF')",        15,         Number("0xF"));

new TestCase( SECTION,  "Number('0X0')",        0,          Number("0X0"));
new TestCase( SECTION,  "Number('0X1')",        1,          Number("0X1"));
new TestCase( SECTION,  "Number('0X2')",        2,          Number("0X2"));
new TestCase( SECTION,  "Number('0X3')",        3,          Number("0X3"));
new TestCase( SECTION,  "Number('0X4')",        4,          Number("0X4"));
new TestCase( SECTION,  "Number('0X5')",        5,          Number("0X5"));
new TestCase( SECTION,  "Number('0X6')",        6,          Number("0X6"));
new TestCase( SECTION,  "Number('0X7')",        7,          Number("0X7"));
new TestCase( SECTION,  "Number('0X8')",        8,          Number("0X8"));
new TestCase( SECTION,  "Number('0X9')",        9,          Number("0X9"));
new TestCase( SECTION,  "Number('0Xa')",        10,         Number("0Xa"));
new TestCase( SECTION,  "Number('0Xb')",        11,         Number("0Xb"));
new TestCase( SECTION,  "Number('0Xc')",        12,         Number("0Xc"));
new TestCase( SECTION,  "Number('0Xd')",        13,         Number("0Xd"));
new TestCase( SECTION,  "Number('0Xe')",        14,         Number("0Xe"));
new TestCase( SECTION,  "Number('0Xf')",        15,         Number("0Xf"));
new TestCase( SECTION,  "Number('0XA')",        10,         Number("0XA"));
new TestCase( SECTION,  "Number('0XB')",        11,         Number("0XB"));
new TestCase( SECTION,  "Number('0XC')",        12,         Number("0XC"));
new TestCase( SECTION,  "Number('0XD')",        13,         Number("0XD"));
new TestCase( SECTION,  "Number('0XE')",        14,         Number("0XE"));
new TestCase( SECTION,  "Number('0XF')",        15,         Number("0XF"));

test();


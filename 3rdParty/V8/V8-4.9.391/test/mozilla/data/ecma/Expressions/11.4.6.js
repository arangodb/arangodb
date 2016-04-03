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

gTestfile = '11.4.6.js';

/**
   File Name:          11.4.6.js
   ECMA Section:       11.4.6 Unary + Operator
   Description:        convert operand to Number type
   Author:             christine@netscape.com
   Date:               7 july 1997
*/

var SECTION = "11.4.6";
var VERSION = "ECMA_1";
var BUGNUMBER="77391";

startTest();

writeHeaderToLog( SECTION + " Unary + operator");

new TestCase( SECTION,  "+('')",           0,      +("") );
new TestCase( SECTION,  "+(' ')",          0,      +(" ") );
new TestCase( SECTION,  "+(\\t)",          0,      +("\t") );
new TestCase( SECTION,  "+(\\n)",          0,      +("\n") );
new TestCase( SECTION,  "+(\\r)",          0,      +("\r") );
new TestCase( SECTION,  "+(\\f)",          0,      +("\f") );

new TestCase( SECTION,  "+(String.fromCharCode(0x0009)",   0,  +(String.fromCharCode(0x0009)) );
new TestCase( SECTION,  "+(String.fromCharCode(0x0020)",   0,  +(String.fromCharCode(0x0020)) );
new TestCase( SECTION,  "+(String.fromCharCode(0x000C)",   0,  +(String.fromCharCode(0x000C)) );
new TestCase( SECTION,  "+(String.fromCharCode(0x000B)",   0,  +(String.fromCharCode(0x000B)) );
new TestCase( SECTION,  "+(String.fromCharCode(0x000D)",   0,  +(String.fromCharCode(0x000D)) );
new TestCase( SECTION,  "+(String.fromCharCode(0x000A)",   0,  +(String.fromCharCode(0x000A)) );

//  a StringNumericLiteral may be preceeded or followed by whitespace and/or
//  line terminators

new TestCase( SECTION,  "+( '   ' +  999 )",        999,    +( '   '+999) );
new TestCase( SECTION,  "+( '\\n'  + 999 )",       999,    +( '\n' +999) );
new TestCase( SECTION,  "+( '\\r'  + 999 )",       999,    +( '\r' +999) );
new TestCase( SECTION,  "+( '\\t'  + 999 )",       999,    +( '\t' +999) );
new TestCase( SECTION,  "+( '\\f'  + 999 )",       999,    +( '\f' +999) );

new TestCase( SECTION,  "+( 999 + '   ' )",        999,    +( 999+'   ') );
new TestCase( SECTION,  "+( 999 + '\\n' )",        999,    +( 999+'\n' ) );
new TestCase( SECTION,  "+( 999 + '\\r' )",        999,    +( 999+'\r' ) );
new TestCase( SECTION,  "+( 999 + '\\t' )",        999,    +( 999+'\t' ) );
new TestCase( SECTION,  "+( 999 + '\\f' )",        999,    +( 999+'\f' ) );

new TestCase( SECTION,  "+( '\\n'  + 999 + '\\n' )",         999,    +( '\n' +999+'\n' ) );
new TestCase( SECTION,  "+( '\\r'  + 999 + '\\r' )",         999,    +( '\r' +999+'\r' ) );
new TestCase( SECTION,  "+( '\\t'  + 999 + '\\t' )",         999,    +( '\t' +999+'\t' ) );
new TestCase( SECTION,  "+( '\\f'  + 999 + '\\f' )",         999,    +( '\f' +999+'\f' ) );

new TestCase( SECTION,  "+( '   ' +  '999' )",     999,    +( '   '+'999') );
new TestCase( SECTION,  "+( '\\n'  + '999' )",       999,    +( '\n' +'999') );
new TestCase( SECTION,  "+( '\\r'  + '999' )",       999,    +( '\r' +'999') );
new TestCase( SECTION,  "+( '\\t'  + '999' )",       999,    +( '\t' +'999') );
new TestCase( SECTION,  "+( '\\f'  + '999' )",       999,    +( '\f' +'999') );

new TestCase( SECTION,  "+( '999' + '   ' )",        999,    +( '999'+'   ') );
new TestCase( SECTION,  "+( '999' + '\\n' )",        999,    +( '999'+'\n' ) );
new TestCase( SECTION,  "+( '999' + '\\r' )",        999,    +( '999'+'\r' ) );
new TestCase( SECTION,  "+( '999' + '\\t' )",        999,    +( '999'+'\t' ) );
new TestCase( SECTION,  "+( '999' + '\\f' )",        999,    +( '999'+'\f' ) );

new TestCase( SECTION,  "+( '\\n'  + '999' + '\\n' )",         999,    +( '\n' +'999'+'\n' ) );
new TestCase( SECTION,  "+( '\\r'  + '999' + '\\r' )",         999,    +( '\r' +'999'+'\r' ) );
new TestCase( SECTION,  "+( '\\t'  + '999' + '\\t' )",         999,    +( '\t' +'999'+'\t' ) );
new TestCase( SECTION,  "+( '\\f'  + '999' + '\\f' )",         999,    +( '\f' +'999'+'\f' ) );

new TestCase( SECTION,  "+( String.fromCharCode(0x0009) +  '99' )",    99,     +( String.fromCharCode(0x0009) +  '99' ) );
new TestCase( SECTION,  "+( String.fromCharCode(0x0020) +  '99' )",    99,     +( String.fromCharCode(0x0020) +  '99' ) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000C) +  '99' )",    99,     +( String.fromCharCode(0x000C) +  '99' ) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000B) +  '99' )",    99,     +( String.fromCharCode(0x000B) +  '99' ) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000D) +  '99' )",    99,     +( String.fromCharCode(0x000D) +  '99' ) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000A) +  '99' )",    99,     +( String.fromCharCode(0x000A) +  '99' ) );

new TestCase( SECTION,  "+( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x0009)",    99,     +( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x0009)) );
new TestCase( SECTION,  "+( String.fromCharCode(0x0020) +  '99' + String.fromCharCode(0x0020)",    99,     +( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x0020)) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000C) +  '99' + String.fromCharCode(0x000C)",    99,     +( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x000C)) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000D) +  '99' + String.fromCharCode(0x000D)",    99,     +( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x000D)) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000B) +  '99' + String.fromCharCode(0x000B)",    99,     +( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x000B)) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000A) +  '99' + String.fromCharCode(0x000A)",    99,     +( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x000A)) );

new TestCase( SECTION,  "+( '99' + String.fromCharCode(0x0009)",    99,     +( '99' + String.fromCharCode(0x0009)) );
new TestCase( SECTION,  "+( '99' + String.fromCharCode(0x0020)",    99,     +( '99' + String.fromCharCode(0x0020)) );
new TestCase( SECTION,  "+( '99' + String.fromCharCode(0x000C)",    99,     +( '99' + String.fromCharCode(0x000C)) );
new TestCase( SECTION,  "+( '99' + String.fromCharCode(0x000D)",    99,     +( '99' + String.fromCharCode(0x000D)) );
new TestCase( SECTION,  "+( '99' + String.fromCharCode(0x000B)",    99,     +( '99' + String.fromCharCode(0x000B)) );
new TestCase( SECTION,  "+( '99' + String.fromCharCode(0x000A)",    99,     +( '99' + String.fromCharCode(0x000A)) );

new TestCase( SECTION,  "+( String.fromCharCode(0x0009) +  99 )",    99,     +( String.fromCharCode(0x0009) +  99 ) );
new TestCase( SECTION,  "+( String.fromCharCode(0x0020) +  99 )",    99,     +( String.fromCharCode(0x0020) +  99 ) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000C) +  99 )",    99,     +( String.fromCharCode(0x000C) +  99 ) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000B) +  99 )",    99,     +( String.fromCharCode(0x000B) +  99 ) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000D) +  99 )",    99,     +( String.fromCharCode(0x000D) +  99 ) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000A) +  99 )",    99,     +( String.fromCharCode(0x000A) +  99 ) );

new TestCase( SECTION,  "+( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x0009)",    99,     +( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x0009)) );
new TestCase( SECTION,  "+( String.fromCharCode(0x0020) +  99 + String.fromCharCode(0x0020)",    99,     +( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x0020)) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000C) +  99 + String.fromCharCode(0x000C)",    99,     +( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x000C)) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000D) +  99 + String.fromCharCode(0x000D)",    99,     +( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x000D)) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000B) +  99 + String.fromCharCode(0x000B)",    99,     +( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x000B)) );
new TestCase( SECTION,  "+( String.fromCharCode(0x000A) +  99 + String.fromCharCode(0x000A)",    99,     +( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x000A)) );

new TestCase( SECTION,  "+( 99 + String.fromCharCode(0x0009)",    99,     +( 99 + String.fromCharCode(0x0009)) );
new TestCase( SECTION,  "+( 99 + String.fromCharCode(0x0020)",    99,     +( 99 + String.fromCharCode(0x0020)) );
new TestCase( SECTION,  "+( 99 + String.fromCharCode(0x000C)",    99,     +( 99 + String.fromCharCode(0x000C)) );
new TestCase( SECTION,  "+( 99 + String.fromCharCode(0x000D)",    99,     +( 99 + String.fromCharCode(0x000D)) );
new TestCase( SECTION,  "+( 99 + String.fromCharCode(0x000B)",    99,     +( 99 + String.fromCharCode(0x000B)) );
new TestCase( SECTION,  "+( 99 + String.fromCharCode(0x000A)",    99,     +( 99 + String.fromCharCode(0x000A)) );


// StrNumericLiteral:::StrDecimalLiteral:::Infinity

new TestCase( SECTION,  "+('Infinity')",   Math.pow(10,10000),   +("Infinity") );
new TestCase( SECTION,  "+('-Infinity')", -Math.pow(10,10000),   +("-Infinity") );
new TestCase( SECTION,  "+('+Infinity')",  Math.pow(10,10000),   +("+Infinity") );

// StrNumericLiteral:::   StrDecimalLiteral ::: DecimalDigits . DecimalDigits opt ExponentPart opt

new TestCase( SECTION,  "+('0')",          0,          +("0") );
new TestCase( SECTION,  "+('-0')",         -0,         +("-0") );
new TestCase( SECTION,  "+('+0')",          0,         +("+0") );

new TestCase( SECTION,  "+('1')",          1,          +("1") );
new TestCase( SECTION,  "+('-1')",         -1,         +("-1") );
new TestCase( SECTION,  "+('+1')",          1,         +("+1") );

new TestCase( SECTION,  "+('2')",          2,          +("2") );
new TestCase( SECTION,  "+('-2')",         -2,         +("-2") );
new TestCase( SECTION,  "+('+2')",          2,         +("+2") );

new TestCase( SECTION,  "+('3')",          3,          +("3") );
new TestCase( SECTION,  "+('-3')",         -3,         +("-3") );
new TestCase( SECTION,  "+('+3')",          3,         +("+3") );

new TestCase( SECTION,  "+('4')",          4,          +("4") );
new TestCase( SECTION,  "+('-4')",         -4,         +("-4") );
new TestCase( SECTION,  "+('+4')",          4,         +("+4") );

new TestCase( SECTION,  "+('5')",          5,          +("5") );
new TestCase( SECTION,  "+('-5')",         -5,         +("-5") );
new TestCase( SECTION,  "+('+5')",          5,         +("+5") );

new TestCase( SECTION,  "+('6')",          6,          +("6") );
new TestCase( SECTION,  "+('-6')",         -6,         +("-6") );
new TestCase( SECTION,  "+('+6')",          6,         +("+6") );

new TestCase( SECTION,  "+('7')",          7,          +("7") );
new TestCase( SECTION,  "+('-7')",         -7,         +("-7") );
new TestCase( SECTION,  "+('+7')",          7,         +("+7") );

new TestCase( SECTION,  "+('8')",          8,          +("8") );
new TestCase( SECTION,  "+('-8')",         -8,         +("-8") );
new TestCase( SECTION,  "+('+8')",          8,         +("+8") );

new TestCase( SECTION,  "+('9')",          9,          +("9") );
new TestCase( SECTION,  "+('-9')",         -9,         +("-9") );
new TestCase( SECTION,  "+('+9')",          9,         +("+9") );

new TestCase( SECTION,  "+('3.14159')",    3.14159,    +("3.14159") );
new TestCase( SECTION,  "+('-3.14159')",   -3.14159,   +("-3.14159") );
new TestCase( SECTION,  "+('+3.14159')",   3.14159,    +("+3.14159") );

new TestCase( SECTION,  "+('3.')",         3,          +("3.") );
new TestCase( SECTION,  "+('-3.')",        -3,         +("-3.") );
new TestCase( SECTION,  "+('+3.')",        3,          +("+3.") );

new TestCase( SECTION,  "+('3.e1')",       30,         +("3.e1") );
new TestCase( SECTION,  "+('-3.e1')",      -30,        +("-3.e1") );
new TestCase( SECTION,  "+('+3.e1')",      30,         +("+3.e1") );

new TestCase( SECTION,  "+('3.e+1')",       30,         +("3.e+1") );
new TestCase( SECTION,  "+('-3.e+1')",      -30,        +("-3.e+1") );
new TestCase( SECTION,  "+('+3.e+1')",      30,         +("+3.e+1") );

new TestCase( SECTION,  "+('3.e-1')",       .30,         +("3.e-1") );
new TestCase( SECTION,  "+('-3.e-1')",      -.30,        +("-3.e-1") );
new TestCase( SECTION,  "+('+3.e-1')",      .30,         +("+3.e-1") );

// StrDecimalLiteral:::  .DecimalDigits ExponentPart opt

new TestCase( SECTION,  "+('.00001')",     0.00001,    +(".00001") );
new TestCase( SECTION,  "+('+.00001')",    0.00001,    +("+.00001") );
new TestCase( SECTION,  "+('-0.0001')",    -0.00001,   +("-.00001") );

new TestCase( SECTION,  "+('.01e2')",      1,          +(".01e2") );
new TestCase( SECTION,  "+('+.01e2')",     1,          +("+.01e2") );
new TestCase( SECTION,  "+('-.01e2')",     -1,         +("-.01e2") );

new TestCase( SECTION,  "+('.01e+2')",      1,         +(".01e+2") );
new TestCase( SECTION,  "+('+.01e+2')",     1,         +("+.01e+2") );
new TestCase( SECTION,  "+('-.01e+2')",     -1,        +("-.01e+2") );

new TestCase( SECTION,  "+('.01e-2')",      0.0001,    +(".01e-2") );
new TestCase( SECTION,  "+('+.01e-2')",     0.0001,    +("+.01e-2") );
new TestCase( SECTION,  "+('-.01e-2')",     -0.0001,   +("-.01e-2") );

//  StrDecimalLiteral:::    DecimalDigits ExponentPart opt

new TestCase( SECTION,  "+('1234e5')",     123400000,  +("1234e5") );
new TestCase( SECTION,  "+('+1234e5')",    123400000,  +("+1234e5") );
new TestCase( SECTION,  "+('-1234e5')",    -123400000, +("-1234e5") );

new TestCase( SECTION,  "+('1234e+5')",    123400000,  +("1234e+5") );
new TestCase( SECTION,  "+('+1234e+5')",   123400000,  +("+1234e+5") );
new TestCase( SECTION,  "+('-1234e+5')",   -123400000, +("-1234e+5") );

new TestCase( SECTION,  "+('1234e-5')",     0.01234,  +("1234e-5") );
new TestCase( SECTION,  "+('+1234e-5')",    0.01234,  +("+1234e-5") );
new TestCase( SECTION,  "+('-1234e-5')",    -0.01234, +("-1234e-5") );

// StrNumericLiteral::: HexIntegerLiteral

new TestCase( SECTION,  "+('0x0')",        0,          +("0x0"));
new TestCase( SECTION,  "+('0x1')",        1,          +("0x1"));
new TestCase( SECTION,  "+('0x2')",        2,          +("0x2"));
new TestCase( SECTION,  "+('0x3')",        3,          +("0x3"));
new TestCase( SECTION,  "+('0x4')",        4,          +("0x4"));
new TestCase( SECTION,  "+('0x5')",        5,          +("0x5"));
new TestCase( SECTION,  "+('0x6')",        6,          +("0x6"));
new TestCase( SECTION,  "+('0x7')",        7,          +("0x7"));
new TestCase( SECTION,  "+('0x8')",        8,          +("0x8"));
new TestCase( SECTION,  "+('0x9')",        9,          +("0x9"));
new TestCase( SECTION,  "+('0xa')",        10,         +("0xa"));
new TestCase( SECTION,  "+('0xb')",        11,         +("0xb"));
new TestCase( SECTION,  "+('0xc')",        12,         +("0xc"));
new TestCase( SECTION,  "+('0xd')",        13,         +("0xd"));
new TestCase( SECTION,  "+('0xe')",        14,         +("0xe"));
new TestCase( SECTION,  "+('0xf')",        15,         +("0xf"));
new TestCase( SECTION,  "+('0xA')",        10,         +("0xA"));
new TestCase( SECTION,  "+('0xB')",        11,         +("0xB"));
new TestCase( SECTION,  "+('0xC')",        12,         +("0xC"));
new TestCase( SECTION,  "+('0xD')",        13,         +("0xD"));
new TestCase( SECTION,  "+('0xE')",        14,         +("0xE"));
new TestCase( SECTION,  "+('0xF')",        15,         +("0xF"));

new TestCase( SECTION,  "+('0X0')",        0,          +("0X0"));
new TestCase( SECTION,  "+('0X1')",        1,          +("0X1"));
new TestCase( SECTION,  "+('0X2')",        2,          +("0X2"));
new TestCase( SECTION,  "+('0X3')",        3,          +("0X3"));
new TestCase( SECTION,  "+('0X4')",        4,          +("0X4"));
new TestCase( SECTION,  "+('0X5')",        5,          +("0X5"));
new TestCase( SECTION,  "+('0X6')",        6,          +("0X6"));
new TestCase( SECTION,  "+('0X7')",        7,          +("0X7"));
new TestCase( SECTION,  "+('0X8')",        8,          +("0X8"));
new TestCase( SECTION,  "+('0X9')",        9,          +("0X9"));
new TestCase( SECTION,  "+('0Xa')",        10,         +("0Xa"));
new TestCase( SECTION,  "+('0Xb')",        11,         +("0Xb"));
new TestCase( SECTION,  "+('0Xc')",        12,         +("0Xc"));
new TestCase( SECTION,  "+('0Xd')",        13,         +("0Xd"));
new TestCase( SECTION,  "+('0Xe')",        14,         +("0Xe"));
new TestCase( SECTION,  "+('0Xf')",        15,         +("0Xf"));
new TestCase( SECTION,  "+('0XA')",        10,         +("0XA"));
new TestCase( SECTION,  "+('0XB')",        11,         +("0XB"));
new TestCase( SECTION,  "+('0XC')",        12,         +("0XC"));
new TestCase( SECTION,  "+('0XD')",        13,         +("0XD"));
new TestCase( SECTION,  "+('0XE')",        14,         +("0XE"));
new TestCase( SECTION,  "+('0XF')",        15,         +("0XF"));

test();

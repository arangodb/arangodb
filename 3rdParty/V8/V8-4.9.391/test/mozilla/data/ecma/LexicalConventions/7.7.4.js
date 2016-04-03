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

gTestfile = '7.7.4.js';

/**
   File Name:          7.7.4.js
   ECMA Section:       7.7.4 String Literals

   Description:        A string literal is zero or more characters enclosed in
   single or double quotes.  Each character may be
   represented by an escape sequence.


   Author:             christine@netscape.com
   Date:               16 september 1997
*/

var SECTION = "7.7.4";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String Literals";

writeHeaderToLog( SECTION + " "+ TITLE);

// StringLiteral:: "" and ''

new TestCase( SECTION, "\"\"",     "",     "" );
new TestCase( SECTION, "\'\'",     "",      '' );

// DoubleStringCharacters:: DoubleStringCharacter :: EscapeSequence :: CharacterEscapeSequence
new TestCase( SECTION, "\\\"",        String.fromCharCode(0x0022),     "\"" );
new TestCase( SECTION, "\\\'",        String.fromCharCode(0x0027),     "\'" );
new TestCase( SECTION, "\\",         String.fromCharCode(0x005C),     "\\" );
new TestCase( SECTION, "\\b",        String.fromCharCode(0x0008),     "\b" );
new TestCase( SECTION, "\\f",        String.fromCharCode(0x000C),     "\f" );
new TestCase( SECTION, "\\n",        String.fromCharCode(0x000A),     "\n" );
new TestCase( SECTION, "\\r",        String.fromCharCode(0x000D),     "\r" );
new TestCase( SECTION, "\\t",        String.fromCharCode(0x0009),     "\t" );
new TestCase( SECTION, "\\v",        String.fromCharCode(0x000B),        "\v" );

// DoubleStringCharacters:DoubleStringCharacter::EscapeSequence::OctalEscapeSequence

new TestCase( SECTION, "\\00",      String.fromCharCode(0x0000),    "\00" );
new TestCase( SECTION, "\\01",      String.fromCharCode(0x0001),    "\01" );
new TestCase( SECTION, "\\02",      String.fromCharCode(0x0002),    "\02" );
new TestCase( SECTION, "\\03",      String.fromCharCode(0x0003),    "\03" );
new TestCase( SECTION, "\\04",      String.fromCharCode(0x0004),    "\04" );
new TestCase( SECTION, "\\05",      String.fromCharCode(0x0005),    "\05" );
new TestCase( SECTION, "\\06",      String.fromCharCode(0x0006),    "\06" );
new TestCase( SECTION, "\\07",      String.fromCharCode(0x0007),    "\07" );

new TestCase( SECTION, "\\010",      String.fromCharCode(0x0008),    "\010" );
new TestCase( SECTION, "\\011",      String.fromCharCode(0x0009),    "\011" );
new TestCase( SECTION, "\\012",      String.fromCharCode(0x000A),    "\012" );
new TestCase( SECTION, "\\013",      String.fromCharCode(0x000B),    "\013" );
new TestCase( SECTION, "\\014",      String.fromCharCode(0x000C),    "\014" );
new TestCase( SECTION, "\\015",      String.fromCharCode(0x000D),    "\015" );
new TestCase( SECTION, "\\016",      String.fromCharCode(0x000E),    "\016" );
new TestCase( SECTION, "\\017",      String.fromCharCode(0x000F),    "\017" );
new TestCase( SECTION, "\\020",      String.fromCharCode(0x0010),    "\020" );
new TestCase( SECTION, "\\042",      String.fromCharCode(0x0022),    "\042" );

new TestCase( SECTION, "\\0",      String.fromCharCode(0x0000),    "\0" );
new TestCase( SECTION, "\\1",      String.fromCharCode(0x0001),    "\1" );
new TestCase( SECTION, "\\2",      String.fromCharCode(0x0002),    "\2" );
new TestCase( SECTION, "\\3",      String.fromCharCode(0x0003),    "\3" );
new TestCase( SECTION, "\\4",      String.fromCharCode(0x0004),    "\4" );
new TestCase( SECTION, "\\5",      String.fromCharCode(0x0005),    "\5" );
new TestCase( SECTION, "\\6",      String.fromCharCode(0x0006),    "\6" );
new TestCase( SECTION, "\\7",      String.fromCharCode(0x0007),    "\7" );

new TestCase( SECTION, "\\10",      String.fromCharCode(0x0008),    "\10" );
new TestCase( SECTION, "\\11",      String.fromCharCode(0x0009),    "\11" );
new TestCase( SECTION, "\\12",      String.fromCharCode(0x000A),    "\12" );
new TestCase( SECTION, "\\13",      String.fromCharCode(0x000B),    "\13" );
new TestCase( SECTION, "\\14",      String.fromCharCode(0x000C),    "\14" );
new TestCase( SECTION, "\\15",      String.fromCharCode(0x000D),    "\15" );
new TestCase( SECTION, "\\16",      String.fromCharCode(0x000E),    "\16" );
new TestCase( SECTION, "\\17",      String.fromCharCode(0x000F),    "\17" );
new TestCase( SECTION, "\\20",      String.fromCharCode(0x0010),    "\20" );
new TestCase( SECTION, "\\42",      String.fromCharCode(0x0022),    "\42" );

new TestCase( SECTION, "\\000",      String.fromCharCode(0),        "\000" );
new TestCase( SECTION, "\\111",      String.fromCharCode(73),       "\111" );
new TestCase( SECTION, "\\222",      String.fromCharCode(146),      "\222" );
new TestCase( SECTION, "\\333",      String.fromCharCode(219),      "\333" );

//  following line commented out as it causes a compile time error
//    new TestCase( SECTION, "\\444",      "444",                         "\444" );

// DoubleStringCharacters:DoubleStringCharacter::EscapeSequence::HexEscapeSequence
/*
  new TestCase( SECTION, "\\x0",      String.fromCharCode(0),         "\x0" );
  new TestCase( SECTION, "\\x1",      String.fromCharCode(1),         "\x1" );
  new TestCase( SECTION, "\\x2",      String.fromCharCode(2),         "\x2" );
  new TestCase( SECTION, "\\x3",      String.fromCharCode(3),         "\x3" );
  new TestCase( SECTION, "\\x4",      String.fromCharCode(4),         "\x4" );
  new TestCase( SECTION, "\\x5",      String.fromCharCode(5),         "\x5" );
  new TestCase( SECTION, "\\x6",      String.fromCharCode(6),         "\x6" );
  new TestCase( SECTION, "\\x7",      String.fromCharCode(7),         "\x7" );
  new TestCase( SECTION, "\\x8",      String.fromCharCode(8),         "\x8" );
  new TestCase( SECTION, "\\x9",      String.fromCharCode(9),         "\x9" );
  new TestCase( SECTION, "\\xA",      String.fromCharCode(10),         "\xA" );
  new TestCase( SECTION, "\\xB",      String.fromCharCode(11),         "\xB" );
  new TestCase( SECTION, "\\xC",      String.fromCharCode(12),         "\xC" );
  new TestCase( SECTION, "\\xD",      String.fromCharCode(13),         "\xD" );
  new TestCase( SECTION, "\\xE",      String.fromCharCode(14),         "\xE" );
  new TestCase( SECTION, "\\xF",      String.fromCharCode(15),         "\xF" );

*/
new TestCase( SECTION, "\\xF0",      String.fromCharCode(240),         "\xF0" );
new TestCase( SECTION, "\\xE1",      String.fromCharCode(225),         "\xE1" );
new TestCase( SECTION, "\\xD2",      String.fromCharCode(210),         "\xD2" );
new TestCase( SECTION, "\\xC3",      String.fromCharCode(195),         "\xC3" );
new TestCase( SECTION, "\\xB4",      String.fromCharCode(180),         "\xB4" );
new TestCase( SECTION, "\\xA5",      String.fromCharCode(165),         "\xA5" );
new TestCase( SECTION, "\\x96",      String.fromCharCode(150),         "\x96" );
new TestCase( SECTION, "\\x87",      String.fromCharCode(135),         "\x87" );
new TestCase( SECTION, "\\x78",      String.fromCharCode(120),         "\x78" );
new TestCase( SECTION, "\\x69",      String.fromCharCode(105),         "\x69" );
new TestCase( SECTION, "\\x5A",      String.fromCharCode(90),         "\x5A" );
new TestCase( SECTION, "\\x4B",      String.fromCharCode(75),         "\x4B" );
new TestCase( SECTION, "\\x3C",      String.fromCharCode(60),         "\x3C" );
new TestCase( SECTION, "\\x2D",      String.fromCharCode(45),         "\x2D" );
new TestCase( SECTION, "\\x1E",      String.fromCharCode(30),         "\x1E" );
new TestCase( SECTION, "\\x0F",      String.fromCharCode(15),         "\x0F" );

// string literals only take up to two hext digits.  therefore, the third character in this string
// should be interpreted as a StringCharacter and not part of the HextEscapeSequence

new TestCase( SECTION, "\\xF0F",      String.fromCharCode(240)+"F",         "\xF0F" );
new TestCase( SECTION, "\\xE1E",      String.fromCharCode(225)+"E",         "\xE1E" );
new TestCase( SECTION, "\\xD2D",      String.fromCharCode(210)+"D",         "\xD2D" );
new TestCase( SECTION, "\\xC3C",      String.fromCharCode(195)+"C",         "\xC3C" );
new TestCase( SECTION, "\\xB4B",      String.fromCharCode(180)+"B",         "\xB4B" );
new TestCase( SECTION, "\\xA5A",      String.fromCharCode(165)+"A",         "\xA5A" );
new TestCase( SECTION, "\\x969",      String.fromCharCode(150)+"9",         "\x969" );
new TestCase( SECTION, "\\x878",      String.fromCharCode(135)+"8",         "\x878" );
new TestCase( SECTION, "\\x787",      String.fromCharCode(120)+"7",         "\x787" );
new TestCase( SECTION, "\\x696",      String.fromCharCode(105)+"6",         "\x696" );
new TestCase( SECTION, "\\x5A5",      String.fromCharCode(90)+"5",         "\x5A5" );
new TestCase( SECTION, "\\x4B4",      String.fromCharCode(75)+"4",         "\x4B4" );
new TestCase( SECTION, "\\x3C3",      String.fromCharCode(60)+"3",         "\x3C3" );
new TestCase( SECTION, "\\x2D2",      String.fromCharCode(45)+"2",         "\x2D2" );
new TestCase( SECTION, "\\x1E1",      String.fromCharCode(30)+"1",         "\x1E1" );
new TestCase( SECTION, "\\x0F0",      String.fromCharCode(15)+"0",         "\x0F0" );

// G is out of hex range

new TestCase( SECTION, "\\xG",        "xG",                                 "\xG" );
new TestCase( SECTION, "\\xCG",       "xCG",      				"\xCG" );

// DoubleStringCharacter::EscapeSequence::CharacterEscapeSequence::\ NonEscapeCharacter
new TestCase( SECTION, "\\a",    "a",        "\a" );
new TestCase( SECTION, "\\c",    "c",        "\c" );
new TestCase( SECTION, "\\d",    "d",        "\d" );
new TestCase( SECTION, "\\e",    "e",        "\e" );
new TestCase( SECTION, "\\g",    "g",        "\g" );
new TestCase( SECTION, "\\h",    "h",        "\h" );
new TestCase( SECTION, "\\i",    "i",        "\i" );
new TestCase( SECTION, "\\j",    "j",        "\j" );
new TestCase( SECTION, "\\k",    "k",        "\k" );
new TestCase( SECTION, "\\l",    "l",        "\l" );
new TestCase( SECTION, "\\m",    "m",        "\m" );
new TestCase( SECTION, "\\o",    "o",        "\o" );
new TestCase( SECTION, "\\p",    "p",        "\p" );
new TestCase( SECTION, "\\q",    "q",        "\q" );
new TestCase( SECTION, "\\s",    "s",        "\s" );
new TestCase( SECTION, "\\u",    "u",        "\u" );

new TestCase( SECTION, "\\w",    "w",        "\w" );
new TestCase( SECTION, "\\x",    "x",        "\x" );
new TestCase( SECTION, "\\y",    "y",        "\y" );
new TestCase( SECTION, "\\z",    "z",        "\z" );
new TestCase( SECTION, "\\9",    "9",        "\9" );

new TestCase( SECTION, "\\A",    "A",        "\A" );
new TestCase( SECTION, "\\B",    "B",        "\B" );
new TestCase( SECTION, "\\C",    "C",        "\C" );
new TestCase( SECTION, "\\D",    "D",        "\D" );
new TestCase( SECTION, "\\E",    "E",        "\E" );
new TestCase( SECTION, "\\F",    "F",        "\F" );
new TestCase( SECTION, "\\G",    "G",        "\G" );
new TestCase( SECTION, "\\H",    "H",        "\H" );
new TestCase( SECTION, "\\I",    "I",        "\I" );
new TestCase( SECTION, "\\J",    "J",        "\J" );
new TestCase( SECTION, "\\K",    "K",        "\K" );
new TestCase( SECTION, "\\L",    "L",        "\L" );
new TestCase( SECTION, "\\M",    "M",        "\M" );
new TestCase( SECTION, "\\N",    "N",        "\N" );
new TestCase( SECTION, "\\O",    "O",        "\O" );
new TestCase( SECTION, "\\P",    "P",        "\P" );
new TestCase( SECTION, "\\Q",    "Q",        "\Q" );
new TestCase( SECTION, "\\R",    "R",        "\R" );
new TestCase( SECTION, "\\S",    "S",        "\S" );
new TestCase( SECTION, "\\T",    "T",        "\T" );
new TestCase( SECTION, "\\U",    "U",        "\U" );
new TestCase( SECTION, "\\V",    "V",        "\V" );
new TestCase( SECTION, "\\W",    "W",        "\W" );
new TestCase( SECTION, "\\X",    "X",        "\X" );
new TestCase( SECTION, "\\Y",    "Y",        "\Y" );
new TestCase( SECTION, "\\Z",    "Z",        "\Z" );

// DoubleStringCharacter::EscapeSequence::UnicodeEscapeSequence

new TestCase( SECTION,  "\\u0020",  " ",        "\u0020" );
new TestCase( SECTION,  "\\u0021",  "!",        "\u0021" );
new TestCase( SECTION,  "\\u0022",  "\"",       "\u0022" );
new TestCase( SECTION,  "\\u0023",  "#",        "\u0023" );
new TestCase( SECTION,  "\\u0024",  "$",        "\u0024" );
new TestCase( SECTION,  "\\u0025",  "%",        "\u0025" );
new TestCase( SECTION,  "\\u0026",  "&",        "\u0026" );
new TestCase( SECTION,  "\\u0027",  "'",        "\u0027" );
new TestCase( SECTION,  "\\u0028",  "(",        "\u0028" );
new TestCase( SECTION,  "\\u0029",  ")",        "\u0029" );
new TestCase( SECTION,  "\\u002A",  "*",        "\u002A" );
new TestCase( SECTION,  "\\u002B",  "+",        "\u002B" );
new TestCase( SECTION,  "\\u002C",  ",",        "\u002C" );
new TestCase( SECTION,  "\\u002D",  "-",        "\u002D" );
new TestCase( SECTION,  "\\u002E",  ".",        "\u002E" );
new TestCase( SECTION,  "\\u002F",  "/",        "\u002F" );
new TestCase( SECTION,  "\\u0030",  "0",        "\u0030" );
new TestCase( SECTION,  "\\u0031",  "1",        "\u0031" );
new TestCase( SECTION,  "\\u0032",  "2",        "\u0032" );
new TestCase( SECTION,  "\\u0033",  "3",        "\u0033" );
new TestCase( SECTION,  "\\u0034",  "4",        "\u0034" );
new TestCase( SECTION,  "\\u0035",  "5",        "\u0035" );
new TestCase( SECTION,  "\\u0036",  "6",        "\u0036" );
new TestCase( SECTION,  "\\u0037",  "7",        "\u0037" );
new TestCase( SECTION,  "\\u0038",  "8",        "\u0038" );
new TestCase( SECTION,  "\\u0039",  "9",        "\u0039" );

test();

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

gTestfile = '15.5.3.2-1.js';

/**
   File Name:          15.5.3.2-1.js
   ECMA Section:       15.5.3.2  String.fromCharCode( char0, char1, ... )
   Description:        Return a string value containing as many characters
   as the number of arguments.  Each argument specifies
   one character of the resulting string, with the first
   argument specifying the first character, and so on,
   from left to right.  An argument is converted to a
   character by applying the operation ToUint16 and
   regarding the resulting 16bit integeras the Unicode
   encoding of a character.  If no arguments are supplied,
   the result is the empty string.

   This test covers Basic Latin (range U+0020 - U+007F)

   Author:             christine@netscape.com
   Date:               2 october 1997
*/

var SECTION = "15.5.3.2-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String.fromCharCode()";

new TestCase( SECTION,   "typeof String.fromCharCode",      "function", typeof String.fromCharCode );
new TestCase( SECTION,   "typeof String.prototype.fromCharCode",        "undefined", typeof String.prototype.fromCharCode );
new TestCase( SECTION,   "var x = new String(); typeof x.fromCharCode", "undefined", eval("var x = new String(); typeof x.fromCharCode") );
new TestCase( SECTION,   "String.fromCharCode.length",      1,      String.fromCharCode.length );

new TestCase( SECTION,    "String.fromCharCode()",          "",     String.fromCharCode() );
new TestCase( SECTION,   "String.fromCharCode(0x0020)",     " ",   String.fromCharCode(0x0020) );
new TestCase( SECTION,   "String.fromCharCode(0x0021)",     "!",   String.fromCharCode(0x0021) );
new TestCase( SECTION,   "String.fromCharCode(0x0022)",     "\"",   String.fromCharCode(0x0022) );
new TestCase( SECTION,   "String.fromCharCode(0x0023)",     "#",   String.fromCharCode(0x0023) );
new TestCase( SECTION,   "String.fromCharCode(0x0024)",     "$",   String.fromCharCode(0x0024) );
new TestCase( SECTION,   "String.fromCharCode(0x0025)",     "%",   String.fromCharCode(0x0025) );
new TestCase( SECTION,   "String.fromCharCode(0x0026)",     "&",   String.fromCharCode(0x0026) );
new TestCase( SECTION,   "String.fromCharCode(0x0027)",     "\'",   String.fromCharCode(0x0027) );
new TestCase( SECTION,   "String.fromCharCode(0x0028)",     "(",   String.fromCharCode(0x0028) );
new TestCase( SECTION,   "String.fromCharCode(0x0029)",     ")",   String.fromCharCode(0x0029) );
new TestCase( SECTION,   "String.fromCharCode(0x002A)",     "*",   String.fromCharCode(0x002A) );
new TestCase( SECTION,   "String.fromCharCode(0x002B)",     "+",   String.fromCharCode(0x002B) );
new TestCase( SECTION,   "String.fromCharCode(0x002C)",     ",",   String.fromCharCode(0x002C) );
new TestCase( SECTION,   "String.fromCharCode(0x002D)",     "-",   String.fromCharCode(0x002D) );
new TestCase( SECTION,   "String.fromCharCode(0x002E)",     ".",   String.fromCharCode(0x002E) );
new TestCase( SECTION,   "String.fromCharCode(0x002F)",     "/",   String.fromCharCode(0x002F) );

new TestCase( SECTION,   "String.fromCharCode(0x0030)",     "0",   String.fromCharCode(0x0030) );
new TestCase( SECTION,   "String.fromCharCode(0x0031)",     "1",   String.fromCharCode(0x0031) );
new TestCase( SECTION,   "String.fromCharCode(0x0032)",     "2",   String.fromCharCode(0x0032) );
new TestCase( SECTION,   "String.fromCharCode(0x0033)",     "3",   String.fromCharCode(0x0033) );
new TestCase( SECTION,   "String.fromCharCode(0x0034)",     "4",   String.fromCharCode(0x0034) );
new TestCase( SECTION,   "String.fromCharCode(0x0035)",     "5",   String.fromCharCode(0x0035) );
new TestCase( SECTION,   "String.fromCharCode(0x0036)",     "6",   String.fromCharCode(0x0036) );
new TestCase( SECTION,   "String.fromCharCode(0x0037)",     "7",   String.fromCharCode(0x0037) );
new TestCase( SECTION,   "String.fromCharCode(0x0038)",     "8",   String.fromCharCode(0x0038) );
new TestCase( SECTION,   "String.fromCharCode(0x0039)",     "9",   String.fromCharCode(0x0039) );
new TestCase( SECTION,   "String.fromCharCode(0x003A)",     ":",   String.fromCharCode(0x003A) );
new TestCase( SECTION,   "String.fromCharCode(0x003B)",     ";",   String.fromCharCode(0x003B) );
new TestCase( SECTION,   "String.fromCharCode(0x003C)",     "<",   String.fromCharCode(0x003C) );
new TestCase( SECTION,   "String.fromCharCode(0x003D)",     "=",   String.fromCharCode(0x003D) );
new TestCase( SECTION,   "String.fromCharCode(0x003E)",     ">",   String.fromCharCode(0x003E) );
new TestCase( SECTION,   "String.fromCharCode(0x003F)",     "?",   String.fromCharCode(0x003F) );

new TestCase( SECTION,   "String.fromCharCode(0x0040)",     "@",   String.fromCharCode(0x0040) );
new TestCase( SECTION,   "String.fromCharCode(0x0041)",     "A",   String.fromCharCode(0x0041) );
new TestCase( SECTION,   "String.fromCharCode(0x0042)",     "B",   String.fromCharCode(0x0042) );
new TestCase( SECTION,   "String.fromCharCode(0x0043)",     "C",   String.fromCharCode(0x0043) );
new TestCase( SECTION,   "String.fromCharCode(0x0044)",     "D",   String.fromCharCode(0x0044) );
new TestCase( SECTION,   "String.fromCharCode(0x0045)",     "E",   String.fromCharCode(0x0045) );
new TestCase( SECTION,   "String.fromCharCode(0x0046)",     "F",   String.fromCharCode(0x0046) );
new TestCase( SECTION,   "String.fromCharCode(0x0047)",     "G",   String.fromCharCode(0x0047) );
new TestCase( SECTION,   "String.fromCharCode(0x0048)",     "H",   String.fromCharCode(0x0048) );
new TestCase( SECTION,   "String.fromCharCode(0x0049)",     "I",   String.fromCharCode(0x0049) );
new TestCase( SECTION,   "String.fromCharCode(0x004A)",     "J",   String.fromCharCode(0x004A) );
new TestCase( SECTION,   "String.fromCharCode(0x004B)",     "K",   String.fromCharCode(0x004B) );
new TestCase( SECTION,   "String.fromCharCode(0x004C)",     "L",   String.fromCharCode(0x004C) );
new TestCase( SECTION,   "String.fromCharCode(0x004D)",     "M",   String.fromCharCode(0x004D) );
new TestCase( SECTION,   "String.fromCharCode(0x004E)",     "N",   String.fromCharCode(0x004E) );
new TestCase( SECTION,   "String.fromCharCode(0x004F)",     "O",   String.fromCharCode(0x004F) );

new TestCase( SECTION,   "String.fromCharCode(0x0040)",     "@",   String.fromCharCode(0x0040) );
new TestCase( SECTION,   "String.fromCharCode(0x0041)",     "A",   String.fromCharCode(0x0041) );
new TestCase( SECTION,   "String.fromCharCode(0x0042)",     "B",   String.fromCharCode(0x0042) );
new TestCase( SECTION,   "String.fromCharCode(0x0043)",     "C",   String.fromCharCode(0x0043) );
new TestCase( SECTION,   "String.fromCharCode(0x0044)",     "D",   String.fromCharCode(0x0044) );
new TestCase( SECTION,   "String.fromCharCode(0x0045)",     "E",   String.fromCharCode(0x0045) );
new TestCase( SECTION,   "String.fromCharCode(0x0046)",     "F",   String.fromCharCode(0x0046) );
new TestCase( SECTION,   "String.fromCharCode(0x0047)",     "G",   String.fromCharCode(0x0047) );
new TestCase( SECTION,   "String.fromCharCode(0x0048)",     "H",   String.fromCharCode(0x0048) );
new TestCase( SECTION,   "String.fromCharCode(0x0049)",     "I",   String.fromCharCode(0x0049) );
new TestCase( SECTION,   "String.fromCharCode(0x004A)",     "J",   String.fromCharCode(0x004A) );
new TestCase( SECTION,   "String.fromCharCode(0x004B)",     "K",   String.fromCharCode(0x004B) );
new TestCase( SECTION,   "String.fromCharCode(0x004C)",     "L",   String.fromCharCode(0x004C) );
new TestCase( SECTION,   "String.fromCharCode(0x004D)",     "M",   String.fromCharCode(0x004D) );
new TestCase( SECTION,   "String.fromCharCode(0x004E)",     "N",   String.fromCharCode(0x004E) );
new TestCase( SECTION,   "String.fromCharCode(0x004F)",     "O",   String.fromCharCode(0x004F) );

new TestCase( SECTION,   "String.fromCharCode(0x0050)",     "P",   String.fromCharCode(0x0050) );
new TestCase( SECTION,   "String.fromCharCode(0x0051)",     "Q",   String.fromCharCode(0x0051) );
new TestCase( SECTION,   "String.fromCharCode(0x0052)",     "R",   String.fromCharCode(0x0052) );
new TestCase( SECTION,   "String.fromCharCode(0x0053)",     "S",   String.fromCharCode(0x0053) );
new TestCase( SECTION,   "String.fromCharCode(0x0054)",     "T",   String.fromCharCode(0x0054) );
new TestCase( SECTION,   "String.fromCharCode(0x0055)",     "U",   String.fromCharCode(0x0055) );
new TestCase( SECTION,   "String.fromCharCode(0x0056)",     "V",   String.fromCharCode(0x0056) );
new TestCase( SECTION,   "String.fromCharCode(0x0057)",     "W",   String.fromCharCode(0x0057) );
new TestCase( SECTION,   "String.fromCharCode(0x0058)",     "X",   String.fromCharCode(0x0058) );
new TestCase( SECTION,   "String.fromCharCode(0x0059)",     "Y",   String.fromCharCode(0x0059) );
new TestCase( SECTION,   "String.fromCharCode(0x005A)",     "Z",   String.fromCharCode(0x005A) );
new TestCase( SECTION,   "String.fromCharCode(0x005B)",     "[",   String.fromCharCode(0x005B) );
new TestCase( SECTION,   "String.fromCharCode(0x005C)",     "\\",   String.fromCharCode(0x005C) );
new TestCase( SECTION,   "String.fromCharCode(0x005D)",     "]",   String.fromCharCode(0x005D) );
new TestCase( SECTION,   "String.fromCharCode(0x005E)",     "^",   String.fromCharCode(0x005E) );
new TestCase( SECTION,   "String.fromCharCode(0x005F)",     "_",   String.fromCharCode(0x005F) );

new TestCase( SECTION,   "String.fromCharCode(0x0060)",     "`",   String.fromCharCode(0x0060) );
new TestCase( SECTION,   "String.fromCharCode(0x0061)",     "a",   String.fromCharCode(0x0061) );
new TestCase( SECTION,   "String.fromCharCode(0x0062)",     "b",   String.fromCharCode(0x0062) );
new TestCase( SECTION,   "String.fromCharCode(0x0063)",     "c",   String.fromCharCode(0x0063) );
new TestCase( SECTION,   "String.fromCharCode(0x0064)",     "d",   String.fromCharCode(0x0064) );
new TestCase( SECTION,   "String.fromCharCode(0x0065)",     "e",   String.fromCharCode(0x0065) );
new TestCase( SECTION,   "String.fromCharCode(0x0066)",     "f",   String.fromCharCode(0x0066) );
new TestCase( SECTION,   "String.fromCharCode(0x0067)",     "g",   String.fromCharCode(0x0067) );
new TestCase( SECTION,   "String.fromCharCode(0x0068)",     "h",   String.fromCharCode(0x0068) );
new TestCase( SECTION,   "String.fromCharCode(0x0069)",     "i",   String.fromCharCode(0x0069) );
new TestCase( SECTION,   "String.fromCharCode(0x006A)",     "j",   String.fromCharCode(0x006A) );
new TestCase( SECTION,   "String.fromCharCode(0x006B)",     "k",   String.fromCharCode(0x006B) );
new TestCase( SECTION,   "String.fromCharCode(0x006C)",     "l",   String.fromCharCode(0x006C) );
new TestCase( SECTION,   "String.fromCharCode(0x006D)",     "m",   String.fromCharCode(0x006D) );
new TestCase( SECTION,   "String.fromCharCode(0x006E)",     "n",   String.fromCharCode(0x006E) );
new TestCase( SECTION,   "String.fromCharCode(0x006F)",     "o",   String.fromCharCode(0x006F) );

new TestCase( SECTION,   "String.fromCharCode(0x0070)",     "p",   String.fromCharCode(0x0070) );
new TestCase( SECTION,   "String.fromCharCode(0x0071)",     "q",   String.fromCharCode(0x0071) );
new TestCase( SECTION,   "String.fromCharCode(0x0072)",     "r",   String.fromCharCode(0x0072) );
new TestCase( SECTION,   "String.fromCharCode(0x0073)",     "s",   String.fromCharCode(0x0073) );
new TestCase( SECTION,   "String.fromCharCode(0x0074)",     "t",   String.fromCharCode(0x0074) );
new TestCase( SECTION,   "String.fromCharCode(0x0075)",     "u",   String.fromCharCode(0x0075) );
new TestCase( SECTION,   "String.fromCharCode(0x0076)",     "v",   String.fromCharCode(0x0076) );
new TestCase( SECTION,   "String.fromCharCode(0x0077)",     "w",   String.fromCharCode(0x0077) );
new TestCase( SECTION,   "String.fromCharCode(0x0078)",     "x",   String.fromCharCode(0x0078) );
new TestCase( SECTION,   "String.fromCharCode(0x0079)",     "y",   String.fromCharCode(0x0079) );
new TestCase( SECTION,   "String.fromCharCode(0x007A)",     "z",   String.fromCharCode(0x007A) );
new TestCase( SECTION,   "String.fromCharCode(0x007B)",     "{",   String.fromCharCode(0x007B) );
new TestCase( SECTION,   "String.fromCharCode(0x007C)",     "|",   String.fromCharCode(0x007C) );
new TestCase( SECTION,   "String.fromCharCode(0x007D)",     "}",   String.fromCharCode(0x007D) );
new TestCase( SECTION,   "String.fromCharCode(0x007E)",     "~",   String.fromCharCode(0x007E) );
//    new TestCase( SECTION,   "String.fromCharCode(0x0020, 0x007F)",     "",   String.fromCharCode(0x0040, 0x007F) );

test();

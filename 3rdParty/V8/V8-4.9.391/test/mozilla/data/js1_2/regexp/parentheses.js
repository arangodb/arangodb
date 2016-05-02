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

gTestfile = 'parentheses.js';

/**
   Filename:     parentheses.js
   Description:  'Tests regular expressions containing ()'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: ()';

writeHeaderToLog('Executing script: parentheses.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'abc'.match(new RegExp('(abc)'))
new TestCase ( SECTION, "'abc'.match(new RegExp('(abc)'))",
	       String(["abc","abc"]), String('abc'.match(new RegExp('(abc)'))));

// 'abcdefg'.match(new RegExp('a(bc)d(ef)g'))
new TestCase ( SECTION, "'abcdefg'.match(new RegExp('a(bc)d(ef)g'))",
	       String(["abcdefg","bc","ef"]), String('abcdefg'.match(new RegExp('a(bc)d(ef)g'))));

// 'abcdefg'.match(new RegExp('(.{3})(.{4})'))
new TestCase ( SECTION, "'abcdefg'.match(new RegExp('(.{3})(.{4})'))",
	       String(["abcdefg","abc","defg"]), String('abcdefg'.match(new RegExp('(.{3})(.{4})'))));

// 'aabcdaabcd'.match(new RegExp('(aa)bcd\1'))
new TestCase ( SECTION, "'aabcdaabcd'.match(new RegExp('(aa)bcd\\1'))",
	       String(["aabcdaa","aa"]), String('aabcdaabcd'.match(new RegExp('(aa)bcd\\1'))));

// 'aabcdaabcd'.match(new RegExp('(aa).+\1'))
new TestCase ( SECTION, "'aabcdaabcd'.match(new RegExp('(aa).+\\1'))",
	       String(["aabcdaa","aa"]), String('aabcdaabcd'.match(new RegExp('(aa).+\\1'))));

// 'aabcdaabcd'.match(new RegExp('(.{2}).+\1'))
new TestCase ( SECTION, "'aabcdaabcd'.match(new RegExp('(.{2}).+\\1'))",
	       String(["aabcdaa","aa"]), String('aabcdaabcd'.match(new RegExp('(.{2}).+\\1'))));

// '123456123456'.match(new RegExp('(\d{3})(\d{3})\1\2'))
new TestCase ( SECTION, "'123456123456'.match(new RegExp('(\\d{3})(\\d{3})\\1\\2'))",
	       String(["123456123456","123","456"]), String('123456123456'.match(new RegExp('(\\d{3})(\\d{3})\\1\\2'))));

// 'abcdefg'.match(new RegExp('a(..(..)..)'))
new TestCase ( SECTION, "'abcdefg'.match(new RegExp('a(..(..)..)'))",
	       String(["abcdefg","bcdefg","de"]), String('abcdefg'.match(new RegExp('a(..(..)..)'))));

// 'abcdefg'.match(/a(..(..)..)/)
new TestCase ( SECTION, "'abcdefg'.match(/a(..(..)..)/)",
	       String(["abcdefg","bcdefg","de"]), String('abcdefg'.match(/a(..(..)..)/)));

// 'xabcdefg'.match(new RegExp('(a(b(c)))(d(e(f)))'))
new TestCase ( SECTION, "'xabcdefg'.match(new RegExp('(a(b(c)))(d(e(f)))'))",
	       String(["abcdef","abc","bc","c","def","ef","f"]), String('xabcdefg'.match(new RegExp('(a(b(c)))(d(e(f)))'))));

// 'xabcdefbcefg'.match(new RegExp('(a(b(c)))(d(e(f)))\2\5'))
new TestCase ( SECTION, "'xabcdefbcefg'.match(new RegExp('(a(b(c)))(d(e(f)))\\2\\5'))",
	       String(["abcdefbcef","abc","bc","c","def","ef","f"]), String('xabcdefbcefg'.match(new RegExp('(a(b(c)))(d(e(f)))\\2\\5'))));

// 'abcd'.match(new RegExp('a(.?)b\1c\1d\1'))
new TestCase ( SECTION, "'abcd'.match(new RegExp('a(.?)b\\1c\\1d\\1'))",
	       String(["abcd",""]), String('abcd'.match(new RegExp('a(.?)b\\1c\\1d\\1'))));

// 'abcd'.match(/a(.?)b\1c\1d\1/)
new TestCase ( SECTION, "'abcd'.match(/a(.?)b\\1c\\1d\\1/)",
	       String(["abcd",""]), String('abcd'.match(/a(.?)b\1c\1d\1/)));

test();

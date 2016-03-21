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

gTestfile = 'character_class.js';

/**
   Filename:     character_class.js
   Description:  'Tests regular expressions containing []'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'RegExp: []';

writeHeaderToLog('Executing script: character_class.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'abcde'.match(new RegExp('ab[ercst]de'))
new TestCase ( SECTION, "'abcde'.match(new RegExp('ab[ercst]de'))",
	       String(["abcde"]), String('abcde'.match(new RegExp('ab[ercst]de'))));

// 'abcde'.match(new RegExp('ab[erst]de'))
new TestCase ( SECTION, "'abcde'.match(new RegExp('ab[erst]de'))",
	       null, 'abcde'.match(new RegExp('ab[erst]de')));

// 'abcdefghijkl'.match(new RegExp('[d-h]+'))
new TestCase ( SECTION, "'abcdefghijkl'.match(new RegExp('[d-h]+'))",
	       String(["defgh"]), String('abcdefghijkl'.match(new RegExp('[d-h]+'))));

// 'abc6defghijkl'.match(new RegExp('[1234567].{2}'))
new TestCase ( SECTION, "'abc6defghijkl'.match(new RegExp('[1234567].{2}'))",
	       String(["6de"]), String('abc6defghijkl'.match(new RegExp('[1234567].{2}'))));

// '\n\n\abc324234\n'.match(new RegExp('[a-c\d]+'))
new TestCase ( SECTION, "'\n\n\abc324234\n'.match(new RegExp('[a-c\\d]+'))",
	       String(["abc324234"]), String('\n\n\abc324234\n'.match(new RegExp('[a-c\\d]+'))));

// 'abc'.match(new RegExp('ab[.]?c'))
new TestCase ( SECTION, "'abc'.match(new RegExp('ab[.]?c'))",
	       String(["abc"]), String('abc'.match(new RegExp('ab[.]?c'))));

// 'abc'.match(new RegExp('a[b]c'))
new TestCase ( SECTION, "'abc'.match(new RegExp('a[b]c'))",
	       String(["abc"]), String('abc'.match(new RegExp('a[b]c'))));

// 'a1b  b2c  c3d  def  f4g'.match(new RegExp('[a-z][^1-9][a-z]'))
new TestCase ( SECTION, "'a1b  b2c  c3d  def  f4g'.match(new RegExp('[a-z][^1-9][a-z]'))",
	       String(["def"]), String('a1b  b2c  c3d  def  f4g'.match(new RegExp('[a-z][^1-9][a-z]'))));

// '123*&$abc'.match(new RegExp('[*&$]{3}'))
new TestCase ( SECTION, "'123*&$abc'.match(new RegExp('[*&$]{3}'))",
	       String(["*&$"]), String('123*&$abc'.match(new RegExp('[*&$]{3}'))));

// 'abc'.match(new RegExp('a[^1-9]c'))
new TestCase ( SECTION, "'abc'.match(new RegExp('a[^1-9]c'))",
	       String(["abc"]), String('abc'.match(new RegExp('a[^1-9]c'))));

// 'abc'.match(new RegExp('a[^b]c'))
new TestCase ( SECTION, "'abc'.match(new RegExp('a[^b]c'))",
	       null, 'abc'.match(new RegExp('a[^b]c')));

// 'abc#$%def%&*@ghi)(*&'.match(new RegExp('[^a-z]{4}'))
new TestCase ( SECTION, "'abc#$%def%&*@ghi)(*&'.match(new RegExp('[^a-z]{4}'))",
	       String(["%&*@"]), String('abc#$%def%&*@ghi)(*&'.match(new RegExp('[^a-z]{4}'))));

// 'abc#$%def%&*@ghi)(*&'.match(/[^a-z]{4}/)
new TestCase ( SECTION, "'abc#$%def%&*@ghi)(*&'.match(/[^a-z]{4}/)",
	       String(["%&*@"]), String('abc#$%def%&*@ghi)(*&'.match(/[^a-z]{4}/)));

test();

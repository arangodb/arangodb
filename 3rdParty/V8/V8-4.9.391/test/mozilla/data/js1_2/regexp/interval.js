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

gTestfile = 'interval.js';

/**
   Filename:     interval.js
   Description:  'Tests regular expressions containing {}'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: {}';

writeHeaderToLog('Executing script: interval.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{2}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{2}c'))",
	       String(["bbc"]), String('aaabbbbcccddeeeefffff'.match(new RegExp('b{2}c'))));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{8}'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{8}'))",
	       null, 'aaabbbbcccddeeeefffff'.match(new RegExp('b{8}')));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{2,}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{2,}c'))",
	       String(["bbbbc"]), String('aaabbbbcccddeeeefffff'.match(new RegExp('b{2,}c'))));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{8,}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{8,}c'))",
	       null, 'aaabbbbcccddeeeefffff'.match(new RegExp('b{8,}c')));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{2,3}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{2,3}c'))",
	       String(["bbbc"]), String('aaabbbbcccddeeeefffff'.match(new RegExp('b{2,3}c'))));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{42,93}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{42,93}c'))",
	       null, 'aaabbbbcccddeeeefffff'.match(new RegExp('b{42,93}c')));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{0,93}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{0,93}c'))",
	       String(["bbbbc"]), String('aaabbbbcccddeeeefffff'.match(new RegExp('b{0,93}c'))));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('bx{0,93}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('bx{0,93}c'))",
	       String(["bc"]), String('aaabbbbcccddeeeefffff'.match(new RegExp('bx{0,93}c'))));

// 'weirwerdf'.match(new RegExp('.{0,93}'))
new TestCase ( SECTION, "'weirwerdf'.match(new RegExp('.{0,93}'))",
	       String(["weirwerdf"]), String('weirwerdf'.match(new RegExp('.{0,93}'))));

// 'wqe456646dsff'.match(new RegExp('\d{1,}'))
new TestCase ( SECTION, "'wqe456646dsff'.match(new RegExp('\\d{1,}'))",
	       String(["456646"]), String('wqe456646dsff'.match(new RegExp('\\d{1,}'))));

// '123123'.match(new RegExp('(123){1,}'))
new TestCase ( SECTION, "'123123'.match(new RegExp('(123){1,}'))",
	       String(["123123","123"]), String('123123'.match(new RegExp('(123){1,}'))));

// '123123x123'.match(new RegExp('(123){1,}x\1'))
new TestCase ( SECTION, "'123123x123'.match(new RegExp('(123){1,}x\\1'))",
	       String(["123123x123","123"]), String('123123x123'.match(new RegExp('(123){1,}x\\1'))));

// '123123x123'.match(/(123){1,}x\1/)
new TestCase ( SECTION, "'123123x123'.match(/(123){1,}x\\1/)",
	       String(["123123x123","123"]), String('123123x123'.match(/(123){1,}x\1/)));

// 'xxxxxxx'.match(new RegExp('x{1,2}x{1,}'))
new TestCase ( SECTION, "'xxxxxxx'.match(new RegExp('x{1,2}x{1,}'))",
	       String(["xxxxxxx"]), String('xxxxxxx'.match(new RegExp('x{1,2}x{1,}'))));

// 'xxxxxxx'.match(/x{1,2}x{1,}/)
new TestCase ( SECTION, "'xxxxxxx'.match(/x{1,2}x{1,}/)",
	       String(["xxxxxxx"]), String('xxxxxxx'.match(/x{1,2}x{1,}/)));

test();

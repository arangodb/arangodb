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

gTestfile = 'vertical_bar.js';

/**
   Filename:     vertical_bar.js
   Description:  'Tests regular expressions containing |'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: |';

writeHeaderToLog('Executing script: vertical_bar.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'abc'.match(new RegExp('xyz|abc'))
new TestCase ( SECTION, "'abc'.match(new RegExp('xyz|abc'))",
	       String(["abc"]), String('abc'.match(new RegExp('xyz|abc'))));

// 'this is a test'.match(new RegExp('quiz|exam|test|homework'))
new TestCase ( SECTION, "'this is a test'.match(new RegExp('quiz|exam|test|homework'))",
	       String(["test"]), String('this is a test'.match(new RegExp('quiz|exam|test|homework'))));

// 'abc'.match(new RegExp('xyz|...'))
new TestCase ( SECTION, "'abc'.match(new RegExp('xyz|...'))",
	       String(["abc"]), String('abc'.match(new RegExp('xyz|...'))));

// 'abc'.match(new RegExp('(.)..|abc'))
new TestCase ( SECTION, "'abc'.match(new RegExp('(.)..|abc'))",
	       String(["abc","a"]), String('abc'.match(new RegExp('(.)..|abc'))));

// 'color: grey'.match(new RegExp('.+: gr(a|e)y'))
new TestCase ( SECTION, "'color: grey'.match(new RegExp('.+: gr(a|e)y'))",
	       String(["color: grey","e"]), String('color: grey'.match(new RegExp('.+: gr(a|e)y'))));

// 'no match'.match(new RegExp('red|white|blue'))
new TestCase ( SECTION, "'no match'.match(new RegExp('red|white|blue'))",
	       null, 'no match'.match(new RegExp('red|white|blue')));

// 'Hi Bob'.match(new RegExp('(Rob)|(Bob)|(Robert)|(Bobby)'))
new TestCase ( SECTION, "'Hi Bob'.match(new RegExp('(Rob)|(Bob)|(Robert)|(Bobby)'))",
	       String(["Bob",undefined,"Bob", undefined, undefined]), String('Hi Bob'.match(new RegExp('(Rob)|(Bob)|(Robert)|(Bobby)'))));

// 'abcdef'.match(new RegExp('abc|bcd|cde|def'))
new TestCase ( SECTION, "'abcdef'.match(new RegExp('abc|bcd|cde|def'))",
	       String(["abc"]), String('abcdef'.match(new RegExp('abc|bcd|cde|def'))));

// 'Hi Bob'.match(/(Rob)|(Bob)|(Robert)|(Bobby)/)
new TestCase ( SECTION, "'Hi Bob'.match(/(Rob)|(Bob)|(Robert)|(Bobby)/)",
	       String(["Bob",undefined,"Bob", undefined, undefined]), String('Hi Bob'.match(/(Rob)|(Bob)|(Robert)|(Bobby)/)));

// 'abcdef'.match(/abc|bcd|cde|def/)
new TestCase ( SECTION, "'abcdef'.match(/abc|bcd|cde|def/)",
	       String(["abc"]), String('abcdef'.match(/abc|bcd|cde|def/)));

test();

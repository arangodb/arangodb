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

gTestfile = 'dot.js';

/**
   Filename:     dot.js
   Description:  'Tests regular expressions containing .'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'RegExp: .';

writeHeaderToLog('Executing script: dot.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'abcde'.match(new RegExp('ab.de'))
new TestCase ( SECTION, "'abcde'.match(new RegExp('ab.de'))",
	       String(["abcde"]), String('abcde'.match(new RegExp('ab.de'))));

// 'line 1\nline 2'.match(new RegExp('.+'))
new TestCase ( SECTION, "'line 1\nline 2'.match(new RegExp('.+'))",
	       String(["line 1"]), String('line 1\nline 2'.match(new RegExp('.+'))));

// 'this is a test'.match(new RegExp('.*a.*'))
new TestCase ( SECTION, "'this is a test'.match(new RegExp('.*a.*'))",
	       String(["this is a test"]), String('this is a test'.match(new RegExp('.*a.*'))));

// 'this is a *&^%$# test'.match(new RegExp('.+'))
new TestCase ( SECTION, "'this is a *&^%$# test'.match(new RegExp('.+'))",
	       String(["this is a *&^%$# test"]), String('this is a *&^%$# test'.match(new RegExp('.+'))));

// '....'.match(new RegExp('.+'))
new TestCase ( SECTION, "'....'.match(new RegExp('.+'))",
	       String(["...."]), String('....'.match(new RegExp('.+'))));

// 'abcdefghijklmnopqrstuvwxyz'.match(new RegExp('.+'))
new TestCase ( SECTION, "'abcdefghijklmnopqrstuvwxyz'.match(new RegExp('.+'))",
	       String(["abcdefghijklmnopqrstuvwxyz"]), String('abcdefghijklmnopqrstuvwxyz'.match(new RegExp('.+'))));

// 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'.match(new RegExp('.+'))
new TestCase ( SECTION, "'ABCDEFGHIJKLMNOPQRSTUVWXYZ'.match(new RegExp('.+'))",
	       String(["ABCDEFGHIJKLMNOPQRSTUVWXYZ"]), String('ABCDEFGHIJKLMNOPQRSTUVWXYZ'.match(new RegExp('.+'))));

// '`1234567890-=~!@#$%^&*()_+'.match(new RegExp('.+'))
new TestCase ( SECTION, "'`1234567890-=~!@#$%^&*()_+'.match(new RegExp('.+'))",
	       String(["`1234567890-=~!@#$%^&*()_+"]), String('`1234567890-=~!@#$%^&*()_+'.match(new RegExp('.+'))));

// '|\\[{]};:"\',<>.?/'.match(new RegExp('.+'))
new TestCase ( SECTION, "'|\\[{]};:\"\',<>.?/'.match(new RegExp('.+'))",
	       String(["|\\[{]};:\"\',<>.?/"]), String('|\\[{]};:\"\',<>.?/'.match(new RegExp('.+'))));

// '|\\[{]};:"\',<>.?/'.match(/.+/)
new TestCase ( SECTION, "'|\\[{]};:\"\',<>.?/'.match(/.+/)",
	       String(["|\\[{]};:\"\',<>.?/"]), String('|\\[{]};:\"\',<>.?/'.match(/.+/)));

test();

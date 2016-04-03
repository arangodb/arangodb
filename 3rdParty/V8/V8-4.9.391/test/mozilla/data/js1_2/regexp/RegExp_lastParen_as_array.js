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

gTestfile = 'RegExp_lastParen_as_array.js';

/**
   Filename:     RegExp_lastParen_as_array.js
   Description:  'Tests RegExps $+ property (same tests as RegExp_lastParen.js but using $+)'

   Author:       Nick Lerissa
   Date:         March 13, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: $+';

writeHeaderToLog('Executing script: RegExp_lastParen_as_array.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'abcd'.match(/(abc)d/); RegExp['$+']
'abcd'.match(/(abc)d/);
new TestCase ( SECTION, "'abcd'.match(/(abc)d/); RegExp['$+']",
	       'abc', RegExp['$+']);

// 'abcd'.match(/(bcd)e/); RegExp['$+']
'abcd'.match(/(bcd)e/);
new TestCase ( SECTION, "'abcd'.match(/(bcd)e/); RegExp['$+']",
	       'abc', RegExp['$+']);

// 'abcdefg'.match(/(a(b(c(d)e)f)g)/); RegExp['$+']
'abcdefg'.match(/(a(b(c(d)e)f)g)/);
new TestCase ( SECTION, "'abcdefg'.match(/(a(b(c(d)e)f)g)/); RegExp['$+']",
	       'd', RegExp['$+']);

// 'abcdefg'.match(new RegExp('(a(b(c(d)e)f)g)')); RegExp['$+']
'abcdefg'.match(new RegExp('(a(b(c(d)e)f)g)'));
new TestCase ( SECTION, "'abcdefg'.match(new RegExp('(a(b(c(d)e)f)g)')); RegExp['$+']",
	       'd', RegExp['$+']);

// 'abcdefg'.match(/(a(b)c)(d(e)f)/); RegExp['$+']
'abcdefg'.match(/(a(b)c)(d(e)f)/);
new TestCase ( SECTION, "'abcdefg'.match(/(a(b)c)(d(e)f)/); RegExp['$+']",
	       'e', RegExp['$+']);

// 'abcdefg'.match(/(^)abc/); RegExp['$+']
'abcdefg'.match(/(^)abc/);
new TestCase ( SECTION, "'abcdefg'.match(/(^)abc/); RegExp['$+']",
	       '', RegExp['$+']);

// 'abcdefg'.match(/(^a)bc/); RegExp['$+']
'abcdefg'.match(/(^a)bc/);
new TestCase ( SECTION, "'abcdefg'.match(/(^a)bc/); RegExp['$+']",
	       'a', RegExp['$+']);

// 'abcdefg'.match(new RegExp('(^a)bc')); RegExp['$+']
'abcdefg'.match(new RegExp('(^a)bc'));
new TestCase ( SECTION, "'abcdefg'.match(new RegExp('(^a)bc')); RegExp['$+']",
	       'a', RegExp['$+']);

// 'abcdefg'.match(/bc/); RegExp['$+']
'abcdefg'.match(/bc/);
new TestCase ( SECTION, "'abcdefg'.match(/bc/); RegExp['$+']",
	       '', RegExp['$+']);

test();

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

gTestfile = 'RegExp_lastMatch_as_array.js';

/**
   Filename:     RegExp_lastMatch_as_array.js
   Description:  'Tests RegExps $& property (same tests as RegExp_lastMatch.js but using $&)'

   Author:       Nick Lerissa
   Date:         March 13, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: $&';

writeHeaderToLog('Executing script: RegExp_lastMatch_as_array.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'foo'.match(/foo/); RegExp['$&']
'foo'.match(/foo/);
new TestCase ( SECTION, "'foo'.match(/foo/); RegExp['$&']",
	       'foo', RegExp['$&']);

// 'foo'.match(new RegExp('foo')); RegExp['$&']
'foo'.match(new RegExp('foo'));
new TestCase ( SECTION, "'foo'.match(new RegExp('foo')); RegExp['$&']",
	       'foo', RegExp['$&']);

// 'xxx'.match(/bar/); RegExp['$&']
'xxx'.match(/bar/);
new TestCase ( SECTION, "'xxx'.match(/bar/); RegExp['$&']",
	       'foo', RegExp['$&']);

// 'xxx'.match(/$/); RegExp['$&']
'xxx'.match(/$/);
new TestCase ( SECTION, "'xxx'.match(/$/); RegExp['$&']",
	       '', RegExp['$&']);

// 'abcdefg'.match(/^..(cd)[a-z]+/); RegExp['$&']
'abcdefg'.match(/^..(cd)[a-z]+/);
new TestCase ( SECTION, "'abcdefg'.match(/^..(cd)[a-z]+/); RegExp['$&']",
	       'abcdefg', RegExp['$&']);

// 'abcdefgabcdefg'.match(/(a(b(c(d)e)f)g)\1/); RegExp['$&']
'abcdefgabcdefg'.match(/(a(b(c(d)e)f)g)\1/);
new TestCase ( SECTION, "'abcdefgabcdefg'.match(/(a(b(c(d)e)f)g)\\1/); RegExp['$&']",
	       'abcdefgabcdefg', RegExp['$&']);

test();

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

gTestfile = 'string_search.js';

/**
   Filename:     string_search.js
   Description:  'Tests the search method on Strings using regular expressions'

   Author:       Nick Lerissa
   Date:         March 12, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'String: search';

writeHeaderToLog('Executing script: string_search.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'abcdefg'.search(/d/)
new TestCase ( SECTION, "'abcdefg'.search(/d/)",
	       3, 'abcdefg'.search(/d/));

// 'abcdefg'.search(/x/)
new TestCase ( SECTION, "'abcdefg'.search(/x/)",
	       -1, 'abcdefg'.search(/x/));

// 'abcdefg123456hijklmn'.search(/\d+/)
new TestCase ( SECTION, "'abcdefg123456hijklmn'.search(/\d+/)",
	       7, 'abcdefg123456hijklmn'.search(/\d+/));

// 'abcdefg123456hijklmn'.search(new RegExp())
new TestCase ( SECTION, "'abcdefg123456hijklmn'.search(new RegExp())",
	       0, 'abcdefg123456hijklmn'.search(new RegExp()));

// 'abc'.search(new RegExp('$'))
new TestCase ( SECTION, "'abc'.search(new RegExp('$'))",
	       3, 'abc'.search(new RegExp('$')));

// 'abc'.search(new RegExp('^'))
new TestCase ( SECTION, "'abc'.search(new RegExp('^'))",
	       0, 'abc'.search(new RegExp('^')));

// 'abc1'.search(/.\d/)
new TestCase ( SECTION, "'abc1'.search(/.\d/)",
	       2, 'abc1'.search(/.\d/));

// 'abc1'.search(/\d{2}/)
new TestCase ( SECTION, "'abc1'.search(/\d{2}/)",
	       -1, 'abc1'.search(/\d{2}/));

test();

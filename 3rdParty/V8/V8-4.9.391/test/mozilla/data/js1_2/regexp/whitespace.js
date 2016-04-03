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

gTestfile = 'whitespace.js';

/**
   Filename:     whitespace.js
   Description:  'Tests regular expressions containing \f\n\r\t\v\s\S\ '

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: \\f\\n\\r\\t\\v\\s\\S ';

writeHeaderToLog('Executing script: whitespace.js');
writeHeaderToLog( SECTION + " "+ TITLE);


var non_whitespace = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ~`!@#$%^&*()-+={[}]|\\:;'<,>./?1234567890" + '"';
var whitespace     = "\f\n\r\t\v ";

// be sure all whitespace is matched by \s
new TestCase ( SECTION,
	       "'" + whitespace + "'.match(new RegExp('\\s+'))",
	       String([whitespace]), String(whitespace.match(new RegExp('\\s+'))));

// be sure all non-whitespace is matched by \S
new TestCase ( SECTION,
	       "'" + non_whitespace + "'.match(new RegExp('\\S+'))",
	       String([non_whitespace]), String(non_whitespace.match(new RegExp('\\S+'))));

// be sure all non-whitespace is not matched by \s
new TestCase ( SECTION,
	       "'" + non_whitespace + "'.match(new RegExp('\\s'))",
	       null, non_whitespace.match(new RegExp('\\s')));

// be sure all whitespace is not matched by \S
new TestCase ( SECTION,
	       "'" + whitespace + "'.match(new RegExp('\\S'))",
	       null, whitespace.match(new RegExp('\\S')));

var s = non_whitespace + whitespace;

// be sure all digits are matched by \s
new TestCase ( SECTION,
	       "'" + s + "'.match(new RegExp('\\s+'))",
	       String([whitespace]), String(s.match(new RegExp('\\s+'))));

s = whitespace + non_whitespace;

// be sure all non-whitespace are matched by \S
new TestCase ( SECTION,
	       "'" + s + "'.match(new RegExp('\\S+'))",
	       String([non_whitespace]), String(s.match(new RegExp('\\S+'))));

// '1233345find me345'.match(new RegExp('[a-z\\s][a-z\\s]+'))
new TestCase ( SECTION, "'1233345find me345'.match(new RegExp('[a-z\\s][a-z\\s]+'))",
	       String(["find me"]), String('1233345find me345'.match(new RegExp('[a-z\\s][a-z\\s]+'))));

var i;

// be sure all whitespace characters match individually
for (i = 0; i < whitespace.length; ++i)
{
  s = 'ab' + whitespace[i] + 'cd';
  new TestCase ( SECTION,
		 "'" + s + "'.match(new RegExp('\\\\s'))",
		 String([whitespace[i]]), String(s.match(new RegExp('\\s'))));
  new TestCase ( SECTION,
		 "'" + s + "'.match(/\s/)",
		 String([whitespace[i]]), String(s.match(/\s/)));
}
// be sure all non_whitespace characters match individually
for (i = 0; i < non_whitespace.length; ++i)
{
  s = '  ' + non_whitespace[i] + '  ';
  new TestCase ( SECTION,
		 "'" + s + "'.match(new RegExp('\\\\S'))",
		 String([non_whitespace[i]]), String(s.match(new RegExp('\\S'))));
  new TestCase ( SECTION,
		 "'" + s + "'.match(/\S/)",
		 String([non_whitespace[i]]), String(s.match(/\S/)));
}


test();

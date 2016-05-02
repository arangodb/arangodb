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

gTestfile = 'digit.js';

/**
   Filename:     digit.js
   Description:  'Tests regular expressions containing \d'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: \\d';

writeHeaderToLog('Executing script: digit.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var non_digits = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\f\n\r\t\v~`!@#$%^&*()-+={[}]|\\:;'<,>./? " + '"';

var digits = "1234567890";

// be sure all digits are matched by \d
new TestCase ( SECTION,
	       "'" + digits + "'.match(new RegExp('\\d+'))",
	       String([digits]), String(digits.match(new RegExp('\\d+'))));

// be sure all non-digits are matched by \D
new TestCase ( SECTION,
	       "'" + non_digits + "'.match(new RegExp('\\D+'))",
	       String([non_digits]), String(non_digits.match(new RegExp('\\D+'))));

// be sure all non-digits are not matched by \d
new TestCase ( SECTION,
	       "'" + non_digits + "'.match(new RegExp('\\d'))",
	       null, non_digits.match(new RegExp('\\d')));

// be sure all digits are not matched by \D
new TestCase ( SECTION,
	       "'" + digits + "'.match(new RegExp('\\D'))",
	       null, digits.match(new RegExp('\\D')));

var s = non_digits + digits;

// be sure all digits are matched by \d
new TestCase ( SECTION,
	       "'" + s + "'.match(new RegExp('\\d+'))",
	       String([digits]), String(s.match(new RegExp('\\d+'))));

var s = digits + non_digits;

// be sure all non-digits are matched by \D
new TestCase ( SECTION,
	       "'" + s + "'.match(new RegExp('\\D+'))",
	       String([non_digits]), String(s.match(new RegExp('\\D+'))));

var i;

// be sure all digits match individually
for (i = 0; i < digits.length; ++i)
{
  s = 'ab' + digits[i] + 'cd';
  new TestCase ( SECTION,
		 "'" + s + "'.match(new RegExp('\\d'))",
		 String([digits[i]]), String(s.match(new RegExp('\\d'))));
  new TestCase ( SECTION,
		 "'" + s + "'.match(/\\d/)",
		 String([digits[i]]), String(s.match(/\d/)));
}
// be sure all non_digits match individually
for (i = 0; i < non_digits.length; ++i)
{
  s = '12' + non_digits[i] + '34';
  new TestCase ( SECTION,
		 "'" + s + "'.match(new RegExp('\\D'))",
		 String([non_digits[i]]), String(s.match(new RegExp('\\D'))));
  new TestCase ( SECTION,
		 "'" + s + "'.match(/\\D/)",
		 String([non_digits[i]]), String(s.match(/\D/)));
}

test();

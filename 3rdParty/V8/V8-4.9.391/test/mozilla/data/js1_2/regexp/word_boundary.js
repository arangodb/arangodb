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

gTestfile = 'word_boundary.js';

/**
   Filename:     word_boundary.js
   Description:  'Tests regular expressions containing \b and \B'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: \\b and \\B';

writeHeaderToLog('Executing script: word_boundary.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'cowboy boyish boy'.match(new RegExp('\bboy\b'))
new TestCase ( SECTION, "'cowboy boyish boy'.match(new RegExp('\\bboy\\b'))",
	       String(["boy"]), String('cowboy boyish boy'.match(new RegExp('\\bboy\\b'))));

var boundary_characters = "\f\n\r\t\v~`!@#$%^&*()-+={[}]|\\:;'<,>./? " + '"';
var non_boundary_characters = '1234567890_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
var s     = '';
var i;

// testing whether all boundary characters are matched when they should be
for (i = 0; i < boundary_characters.length; ++i)
{
  s = '123ab' + boundary_characters.charAt(i) + '123c' + boundary_characters.charAt(i);

  new TestCase ( SECTION,
		 "'" + s + "'.match(new RegExp('\\b123[a-z]\\b'))",
		 String(["123c"]), String(s.match(new RegExp('\\b123[a-z]\\b'))));
}

// testing whether all non-boundary characters are matched when they should be
for (i = 0; i < non_boundary_characters.length; ++i)
{
  s = '123ab' + non_boundary_characters.charAt(i) + '123c' + non_boundary_characters.charAt(i);

  new TestCase ( SECTION,
		 "'" + s + "'.match(new RegExp('\\B123[a-z]\\B'))",
		 String(["123c"]), String(s.match(new RegExp('\\B123[a-z]\\B'))));
}

s = '';

// testing whether all boundary characters are not matched when they should not be
for (i = 0; i < boundary_characters.length; ++i)
{
  s += boundary_characters[i] + "a" + i + "b";
}
s += "xa1111bx";

new TestCase ( SECTION,
	       "'" + s + "'.match(new RegExp('\\Ba\\d+b\\B'))",
	       String(["a1111b"]), String(s.match(new RegExp('\\Ba\\d+b\\B'))));

new TestCase ( SECTION,
	       "'" + s + "'.match(/\\Ba\\d+b\\B/)",
	       String(["a1111b"]), String(s.match(/\Ba\d+b\B/)));

s = '';

// testing whether all non-boundary characters are not matched when they should not be
for (i = 0; i < non_boundary_characters.length; ++i)
{
  s += non_boundary_characters[i] + "a" + i + "b";
}
s += "(a1111b)";

new TestCase ( SECTION,
	       "'" + s + "'.match(new RegExp('\\ba\\d+b\\b'))",
	       String(["a1111b"]), String(s.match(new RegExp('\\ba\\d+b\\b'))));

new TestCase ( SECTION,
	       "'" + s + "'.match(/\\ba\\d+b\\b/)",
	       String(["a1111b"]), String(s.match(/\ba\d+b\b/)));

test();

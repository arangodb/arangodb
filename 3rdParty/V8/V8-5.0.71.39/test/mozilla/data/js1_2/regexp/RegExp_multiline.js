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

gTestfile = 'RegExp_multiline.js';

/**
   Filename:     RegExp_multiline.js
   Description:  'Tests RegExps multiline property'

   Author:       Nick Lerissa
   Date:         March 12, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: multiline';

writeHeaderToLog('Executing script: RegExp_multiline.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// First we do a series of tests with RegExp.multiline set to false (default value)
// Following this we do the same tests with RegExp.multiline set true(**).
// RegExp.multiline
new TestCase ( SECTION, "RegExp.multiline",
	       false, RegExp.multiline);

// (multiline == false) '123\n456'.match(/^4../)
new TestCase ( SECTION, "(multiline == false) '123\\n456'.match(/^4../)",
	       null, '123\n456'.match(/^4../));

// (multiline == false) 'a11\na22\na23\na24'.match(/^a../g)
new TestCase ( SECTION, "(multiline == false) 'a11\\na22\\na23\\na24'.match(/^a../g)",
	       String(['a11']), String('a11\na22\na23\na24'.match(/^a../g)));

// (multiline == false) 'a11\na22'.match(/^.+^./)
new TestCase ( SECTION, "(multiline == false) 'a11\na22'.match(/^.+^./)",
	       null, 'a11\na22'.match(/^.+^./));

// (multiline == false) '123\n456'.match(/.3$/)
new TestCase ( SECTION, "(multiline == false) '123\\n456'.match(/.3$/)",
	       null, '123\n456'.match(/.3$/));

// (multiline == false) 'a11\na22\na23\na24'.match(/a..$/g)
new TestCase ( SECTION, "(multiline == false) 'a11\\na22\\na23\\na24'.match(/a..$/g)",
	       String(['a24']), String('a11\na22\na23\na24'.match(/a..$/g)));

// (multiline == false) 'abc\ndef'.match(/c$...$/)
new TestCase ( SECTION, "(multiline == false) 'abc\ndef'.match(/c$...$/)",
	       null, 'abc\ndef'.match(/c$...$/));

// (multiline == false) 'a11\na22\na23\na24'.match(new RegExp('a..$','g'))
new TestCase ( SECTION, "(multiline == false) 'a11\\na22\\na23\\na24'.match(new RegExp('a..$','g'))",
	       String(['a24']), String('a11\na22\na23\na24'.match(new RegExp('a..$','g'))));

// (multiline == false) 'abc\ndef'.match(new RegExp('c$...$'))
new TestCase ( SECTION, "(multiline == false) 'abc\ndef'.match(new RegExp('c$...$'))",
	       null, 'abc\ndef'.match(new RegExp('c$...$')));

// **Now we do the tests with RegExp.multiline set to true
// RegExp.multiline = true; RegExp.multiline
RegExp.multiline = true;
new TestCase ( SECTION, "RegExp.multiline = true; RegExp.multiline",
	       true, RegExp.multiline);

// (multiline == true) '123\n456'.match(/^4../)
new TestCase ( SECTION, "(multiline == true) '123\\n456'.match(/^4../)",
	       String(['456']), String('123\n456'.match(/^4../)));

// (multiline == true) 'a11\na22\na23\na24'.match(/^a../g)
new TestCase ( SECTION, "(multiline == true) 'a11\\na22\\na23\\na24'.match(/^a../g)",
	       String(['a11','a22','a23','a24']), String('a11\na22\na23\na24'.match(/^a../g)));

// (multiline == true) 'a11\na22'.match(/^.+^./)
//new TestCase ( SECTION, "(multiline == true) 'a11\na22'.match(/^.+^./)",
//                                    String(['a11\na']), String('a11\na22'.match(/^.+^./)));

// (multiline == true) '123\n456'.match(/.3$/)
new TestCase ( SECTION, "(multiline == true) '123\\n456'.match(/.3$/)",
	       String(['23']), String('123\n456'.match(/.3$/)));

// (multiline == true) 'a11\na22\na23\na24'.match(/a..$/g)
new TestCase ( SECTION, "(multiline == true) 'a11\\na22\\na23\\na24'.match(/a..$/g)",
	       String(['a11','a22','a23','a24']), String('a11\na22\na23\na24'.match(/a..$/g)));

// (multiline == true) 'a11\na22\na23\na24'.match(new RegExp('a..$','g'))
new TestCase ( SECTION, "(multiline == true) 'a11\\na22\\na23\\na24'.match(new RegExp('a..$','g'))",
	       String(['a11','a22','a23','a24']), String('a11\na22\na23\na24'.match(new RegExp('a..$','g'))));

// (multiline == true) 'abc\ndef'.match(/c$....$/)
//new TestCase ( SECTION, "(multiline == true) 'abc\ndef'.match(/c$.+$/)",
//                                    'c\ndef', String('abc\ndef'.match(/c$.+$/)));

RegExp.multiline = false;

test();

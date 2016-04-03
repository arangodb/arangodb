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

gTestfile = 'hexadecimal.js';

/**
   Filename:     hexadecimal.js
   Description:  'Tests regular expressions containing \<number> '

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: \x# (hex) ';

writeHeaderToLog('Executing script: hexadecimal.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var testPattern = '\\x41\\x42\\x43\\x44\\x45\\x46\\x47\\x48\\x49\\x4A\\x4B\\x4C\\x4D\\x4E\\x4F\\x50\\x51\\x52\\x53\\x54\\x55\\x56\\x57\\x58\\x59\\x5A';

var testString = "12345ABCDEFGHIJKLMNOPQRSTUVWXYZ67890";

new TestCase ( SECTION,
	       "'" + testString + "'.match(new RegExp('" + testPattern + "'))",
	       String(["ABCDEFGHIJKLMNOPQRSTUVWXYZ"]), String(testString.match(new RegExp(testPattern))));

testPattern = '\\x61\\x62\\x63\\x64\\x65\\x66\\x67\\x68\\x69\\x6A\\x6B\\x6C\\x6D\\x6E\\x6F\\x70\\x71\\x72\\x73\\x74\\x75\\x76\\x77\\x78\\x79\\x7A';

testString = "12345AabcdefghijklmnopqrstuvwxyzZ67890";

new TestCase ( SECTION,
	       "'" + testString + "'.match(new RegExp('" + testPattern + "'))",
	       String(["abcdefghijklmnopqrstuvwxyz"]), String(testString.match(new RegExp(testPattern))));

testPattern = '\\x20\\x21\\x22\\x23\\x24\\x25\\x26\\x27\\x28\\x29\\x2A\\x2B\\x2C\\x2D\\x2E\\x2F\\x30\\x31\\x32\\x33';

testString = "abc !\"#$%&'()*+,-./0123ZBC";

new TestCase ( SECTION,
	       "'" + testString + "'.match(new RegExp('" + testPattern + "'))",
	       String([" !\"#$%&'()*+,-./0123"]), String(testString.match(new RegExp(testPattern))));

testPattern = '\\x34\\x35\\x36\\x37\\x38\\x39\\x3A\\x3B\\x3C\\x3D\\x3E\\x3F\\x40';

testString = "123456789:;<=>?@ABC";

new TestCase ( SECTION,
	       "'" + testString + "'.match(new RegExp('" + testPattern + "'))",
	       String(["456789:;<=>?@"]), String(testString.match(new RegExp(testPattern))));

testPattern = '\\x7B\\x7C\\x7D\\x7E';

testString = "1234{|}~ABC";

new TestCase ( SECTION,
	       "'" + testString + "'.match(new RegExp('" + testPattern + "'))",
	       String(["{|}~"]), String(testString.match(new RegExp(testPattern))));

new TestCase ( SECTION,
	       "'canthisbeFOUND'.match(new RegExp('[A-\\x5A]+'))",
	       String(["FOUND"]), String('canthisbeFOUND'.match(new RegExp('[A-\\x5A]+'))));

new TestCase ( SECTION,
	       "'canthisbeFOUND'.match(new RegExp('[\\x61-\\x7A]+'))",
	       String(["canthisbe"]), String('canthisbeFOUND'.match(new RegExp('[\\x61-\\x7A]+'))));

new TestCase ( SECTION,
	       "'canthisbeFOUND'.match(/[\\x61-\\x7A]+/)",
	       String(["canthisbe"]), String('canthisbeFOUND'.match(/[\x61-\x7A]+/)));

test();

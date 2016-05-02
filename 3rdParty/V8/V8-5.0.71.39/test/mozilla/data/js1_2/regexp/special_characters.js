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

gTestfile = 'special_characters.js';

/**
   Filename:     special_characters.js
   Description:  'Tests regular expressions containing special characters'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: special_charaters';

writeHeaderToLog('Executing script: special_characters.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// testing backslash '\'
new TestCase ( SECTION, "'^abcdefghi'.match(/\^abc/)", String(["^abc"]), String('^abcdefghi'.match(/\^abc/)));

// testing beginning of line '^'
new TestCase ( SECTION, "'abcdefghi'.match(/^abc/)", String(["abc"]), String('abcdefghi'.match(/^abc/)));

// testing end of line '$'
new TestCase ( SECTION, "'abcdefghi'.match(/fghi$/)", String(["ghi"]), String('abcdefghi'.match(/ghi$/)));

// testing repeat '*'
new TestCase ( SECTION, "'eeeefghi'.match(/e*/)", String(["eeee"]), String('eeeefghi'.match(/e*/)));

// testing repeat 1 or more times '+'
new TestCase ( SECTION, "'abcdeeeefghi'.match(/e+/)", String(["eeee"]), String('abcdeeeefghi'.match(/e+/)));

// testing repeat 0 or 1 time '?'
new TestCase ( SECTION, "'abcdefghi'.match(/abc?de/)", String(["abcde"]), String('abcdefghi'.match(/abc?de/)));

// testing any character '.'
new TestCase ( SECTION, "'abcdefghi'.match(/c.e/)", String(["cde"]), String('abcdefghi'.match(/c.e/)));

// testing remembering ()
new TestCase ( SECTION, "'abcewirjskjdabciewjsdf'.match(/(abc).+\\1'/)",
	       String(["abcewirjskjdabc","abc"]), String('abcewirjskjdabciewjsdf'.match(/(abc).+\1/)));

// testing or match '|'
new TestCase ( SECTION, "'abcdefghi'.match(/xyz|def/)", String(["def"]), String('abcdefghi'.match(/xyz|def/)));

// testing repeat n {n}
new TestCase ( SECTION, "'abcdeeeefghi'.match(/e{3}/)", String(["eee"]), String('abcdeeeefghi'.match(/e{3}/)));

// testing min repeat n {n,}
new TestCase ( SECTION, "'abcdeeeefghi'.match(/e{3,}/)", String(["eeee"]), String('abcdeeeefghi'.match(/e{3,}/)));

// testing min/max repeat {min, max}
new TestCase ( SECTION, "'abcdeeeefghi'.match(/e{2,8}/)", String(["eeee"]), String('abcdeeeefghi'.match(/e{2,8}/)));

// testing any in set [abc...]
new TestCase ( SECTION, "'abcdefghi'.match(/cd[xey]fgh/)", String(["cdefgh"]), String('abcdefghi'.match(/cd[xey]fgh/)));

// testing any in set [a-z]
new TestCase ( SECTION, "'netscape inc'.match(/t[r-v]ca/)", String(["tsca"]), String('netscape inc'.match(/t[r-v]ca/)));

// testing any not in set [^abc...]
new TestCase ( SECTION, "'abcdefghi'.match(/cd[^xy]fgh/)", String(["cdefgh"]), String('abcdefghi'.match(/cd[^xy]fgh/)));

// testing any not in set [^a-z]
new TestCase ( SECTION, "'netscape inc'.match(/t[^a-c]ca/)", String(["tsca"]), String('netscape inc'.match(/t[^a-c]ca/)));

// testing backspace [\b]
new TestCase ( SECTION, "'this is b\ba test'.match(/is b[\b]a test/)",
	       String(["is b\ba test"]), String('this is b\ba test'.match(/is b[\b]a test/)));

// testing word boundary \b
new TestCase ( SECTION, "'today is now - day is not now'.match(/\bday.*now/)",
	       String(["day is not now"]), String('today is now - day is not now'.match(/\bday.*now/)));

// control characters???

// testing any digit \d
new TestCase ( SECTION, "'a dog - 1 dog'.match(/\d dog/)", String(["1 dog"]), String('a dog - 1 dog'.match(/\d dog/)));

// testing any non digit \d
new TestCase ( SECTION, "'a dog - 1 dog'.match(/\D dog/)", String(["a dog"]), String('a dog - 1 dog'.match(/\D dog/)));

// testing form feed '\f'
new TestCase ( SECTION, "'a b a\fb'.match(/a\fb/)", String(["a\fb"]), String('a b a\fb'.match(/a\fb/)));

// testing line feed '\n'
new TestCase ( SECTION, "'a b a\nb'.match(/a\nb/)", String(["a\nb"]), String('a b a\nb'.match(/a\nb/)));

// testing carriage return '\r'
new TestCase ( SECTION, "'a b a\rb'.match(/a\rb/)", String(["a\rb"]), String('a b a\rb'.match(/a\rb/)));

// testing whitespace '\s'
new TestCase ( SECTION, "'xa\f\n\r\t\vbz'.match(/a\s+b/)", String(["a\f\n\r\t\vb"]), String('xa\f\n\r\t\vbz'.match(/a\s+b/)));

// testing non whitespace '\S'
new TestCase ( SECTION, "'a\tb a b a-b'.match(/a\Sb/)", String(["a-b"]), String('a\tb a b a-b'.match(/a\Sb/)));

// testing tab '\t'
new TestCase ( SECTION, "'a\t\tb a  b'.match(/a\t{2}/)", String(["a\t\t"]), String('a\t\tb a  b'.match(/a\t{2}/)));

// testing vertical tab '\v'
new TestCase ( SECTION, "'a\v\vb a  b'.match(/a\v{2}/)", String(["a\v\v"]), String('a\v\vb a  b'.match(/a\v{2}/)));

// testing alphnumeric characters '\w'
new TestCase ( SECTION, "'%AZaz09_$'.match(/\w+/)", String(["AZaz09_"]), String('%AZaz09_$'.match(/\w+/)));

// testing non alphnumeric characters '\W'
new TestCase ( SECTION, "'azx$%#@*4534'.match(/\W+/)", String(["$%#@*"]), String('azx$%#@*4534'.match(/\W+/)));

// testing back references '\<number>'
new TestCase ( SECTION, "'test'.match(/(t)es\\1/)", String(["test","t"]), String('test'.match(/(t)es\1/)));

// testing hex excaping with '\'
new TestCase ( SECTION, "'abcdef'.match(/\x63\x64/)", String(["cd"]), String('abcdef'.match(/\x63\x64/)));

// testing oct excaping with '\'
new TestCase ( SECTION, "'abcdef'.match(/\\143\\144/)", String(["cd"]), String('abcdef'.match(/\143\144/)));

test();


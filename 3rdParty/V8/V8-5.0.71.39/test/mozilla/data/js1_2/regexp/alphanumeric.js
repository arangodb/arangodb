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

gTestfile = 'alphanumeric.js';

/**
   Filename:     alphanumeric.js
   Description:  'Tests regular expressions with \w and \W special characters'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: \\w and \\W';

writeHeaderToLog('Executing script: alphanumeric.js');
writeHeaderToLog( SECTION + " " + TITLE);

var non_alphanumeric = "~`!@#$%^&*()-+={[}]|\\:;'<,>./?\f\n\r\t\v " + '"';
var alphanumeric     = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

// be sure all alphanumerics are matched by \w
new TestCase ( SECTION,
	       "'" + alphanumeric + "'.match(new RegExp('\\w+'))",
	       String([alphanumeric]), String(alphanumeric.match(new RegExp('\\w+'))));

// be sure all non-alphanumerics are matched by \W
new TestCase ( SECTION,
	       "'" + non_alphanumeric + "'.match(new RegExp('\\W+'))",
	       String([non_alphanumeric]), String(non_alphanumeric.match(new RegExp('\\W+'))));

// be sure all non-alphanumerics are not matched by \w
new TestCase ( SECTION,
	       "'" + non_alphanumeric + "'.match(new RegExp('\\w'))",
	       null, non_alphanumeric.match(new RegExp('\\w')));

// be sure all alphanumerics are not matched by \W
new TestCase ( SECTION,
	       "'" + alphanumeric + "'.match(new RegExp('\\W'))",
	       null, alphanumeric.match(new RegExp('\\W')));

var s = non_alphanumeric + alphanumeric;

// be sure all alphanumerics are matched by \w
new TestCase ( SECTION,
	       "'" + s + "'.match(new RegExp('\\w+'))",
	       String([alphanumeric]), String(s.match(new RegExp('\\w+'))));

s = alphanumeric + non_alphanumeric;

// be sure all non-alphanumerics are matched by \W
new TestCase ( SECTION,
	       "'" + s + "'.match(new RegExp('\\W+'))",
	       String([non_alphanumeric]), String(s.match(new RegExp('\\W+'))));

// be sure all alphanumerics are matched by \w (using literals)
new TestCase ( SECTION,
	       "'" + s + "'.match(/\w+/)",
	       String([alphanumeric]), String(s.match(/\w+/)));

s = alphanumeric + non_alphanumeric;

// be sure all non-alphanumerics are matched by \W (using literals)
new TestCase ( SECTION,
	       "'" + s + "'.match(/\W+/)",
	       String([non_alphanumeric]), String(s.match(/\W+/)));

s = 'abcd*&^%$$';
// be sure the following test behaves consistently
new TestCase ( SECTION,
	       "'" + s + "'.match(/(\w+)...(\W+)/)",
	       String([s , 'abcd' , '%$$']), String(s.match(/(\w+)...(\W+)/)));

var i;

// be sure all alphanumeric characters match individually
for (i = 0; i < alphanumeric.length; ++i)
{
  s = '#$' + alphanumeric[i] + '%^';
  new TestCase ( SECTION,
		 "'" + s + "'.match(new RegExp('\\w'))",
		 String([alphanumeric[i]]), String(s.match(new RegExp('\\w'))));
}
// be sure all non_alphanumeric characters match individually
for (i = 0; i < non_alphanumeric.length; ++i)
{
  s = 'sd' + non_alphanumeric[i] + String((i+10) * (i+10) - 2 * (i+10));
  new TestCase ( SECTION,
		 "'" + s + "'.match(new RegExp('\\W'))",
		 String([non_alphanumeric[i]]), String(s.match(new RegExp('\\W'))));
}

test();

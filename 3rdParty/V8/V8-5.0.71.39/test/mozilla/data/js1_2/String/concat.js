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

gTestfile = 'concat.js';

/**
   Filename:     concat.js
   Description:  'This tests the new String object method: concat'

   Author:       NickLerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'String:concat';

writeHeaderToLog('Executing script: concat.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var aString = new String("test string");
var bString = new String(" another ");

new TestCase( SECTION, "aString.concat(' more')", "test string more",     aString.concat(' more').toString());
new TestCase( SECTION, "aString.concat(bString)", "test string another ", aString.concat(bString).toString());
new TestCase( SECTION, "aString                ", "test string",          aString.toString());
new TestCase( SECTION, "bString                ", " another ",            bString.toString());
new TestCase( SECTION, "aString.concat(345)    ", "test string345",       aString.concat(345).toString());
new TestCase( SECTION, "aString.concat(true)   ", "test stringtrue",      aString.concat(true).toString());
new TestCase( SECTION, "aString.concat(null)   ", "test stringnull",      aString.concat(null).toString());
new TestCase( SECTION, "aString.concat([])     ", "test string[]",          aString.concat([]).toString());
new TestCase( SECTION, "aString.concat([1,2,3])", "test string[1, 2, 3]",     aString.concat([1,2,3]).toString());

new TestCase( SECTION, "'abcde'.concat(' more')", "abcde more",     'abcde'.concat(' more').toString());
new TestCase( SECTION, "'abcde'.concat(bString)", "abcde another ", 'abcde'.concat(bString).toString());
new TestCase( SECTION, "'abcde'                ", "abcde",          'abcde');
new TestCase( SECTION, "'abcde'.concat(345)    ", "abcde345",       'abcde'.concat(345).toString());
new TestCase( SECTION, "'abcde'.concat(true)   ", "abcdetrue",      'abcde'.concat(true).toString());
new TestCase( SECTION, "'abcde'.concat(null)   ", "abcdenull",      'abcde'.concat(null).toString());
new TestCase( SECTION, "'abcde'.concat([])     ", "abcde[]",          'abcde'.concat([]).toString());
new TestCase( SECTION, "'abcde'.concat([1,2,3])", "abcde[1, 2, 3]",     'abcde'.concat([1,2,3]).toString());

//what should this do:
new TestCase( SECTION, "'abcde'.concat()       ", "abcde",          'abcde'.concat().toString());

test();


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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   pschwartau@netscape.com
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

/*
 * Date: 28 December 2000
 *
 * SUMMARY: Testing regular expressions containing the ? character.
 * Arose from Bugzilla bug 57572: "RegExp with ? matches incorrectly"
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=57572
 *
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-57572.js';
var i = 0;
var BUGNUMBER = 57572;
var summary = 'Testing regular expressions containing "?"';
var cnEmptyString = ''; var cnSingleSpace = ' ';
var status = '';
var statusmessages = new Array();
var pattern = '';
var patterns = new Array();
var string = '';
var strings = new Array();
var actualmatch = '';
var actualmatches = new Array();
var expectedmatch = '';
var expectedmatches = new Array();


status = inSection(1);
pattern = /(\S+)?(.*)/;
string = 'Test this';
actualmatch = string.match(pattern);
expectedmatch = Array(string, 'Test', ' this');  //single space in front of 'this'
addThis();

status = inSection(2);
pattern = /(\S+)? ?(.*)/;  //single space between the ? characters
string= 'Test this';
actualmatch = string.match(pattern);
expectedmatch = Array(string, 'Test', 'this');  //NO space in front of 'this'
addThis();

status = inSection(3);
pattern = /(\S+)?(.*)/;
string = 'Stupid phrase, with six - (short) words';
actualmatch = string.match(pattern);
expectedmatch = Array(string, 'Stupid', ' phrase, with six - (short) words');  //single space in front of 'phrase'
addThis();

status = inSection(4);
pattern = /(\S+)? ?(.*)/;  //single space between the ? characters
string = 'Stupid phrase, with six - (short) words';
actualmatch = string.match(pattern);
expectedmatch = Array(string, 'Stupid', 'phrase, with six - (short) words');  //NO space in front of 'phrase'
addThis();


// let's add an extra back-reference this time - three instead of two -
status = inSection(5);
pattern = /(\S+)?( ?)(.*)/;  //single space before second ? character
string = 'Stupid phrase, with six - (short) words';
actualmatch = string.match(pattern);
expectedmatch = Array(string, 'Stupid', cnSingleSpace, 'phrase, with six - (short) words');
addThis();

status = inSection(6);
pattern = /^(\S+)?( ?)(B+)$/;  //single space before second ? character
string = 'AAABBB';
actualmatch = string.match(pattern);
expectedmatch = Array(string, 'AAABB', cnEmptyString, 'B');
addThis();

status = inSection(7);
pattern = /(\S+)?(!?)(.*)/;
string = 'WOW !!! !!!';
actualmatch = string.match(pattern);
expectedmatch = Array(string, 'WOW', cnEmptyString, ' !!! !!!');
addThis();

status = inSection(8);
pattern = /(.+)?(!?)(!+)/;
string = 'WOW !!! !!!';
actualmatch = string.match(pattern);
expectedmatch = Array(string, 'WOW !!! !!', cnEmptyString, '!');
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusmessages[i] = status;
  patterns[i] = pattern;
  strings[i] = string;
  actualmatches[i] = actualmatch;
  expectedmatches[i] = expectedmatch;
  i++;
}


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
  testRegExp(statusmessages, patterns, strings, actualmatches, expectedmatches);
  exitFunc ('test');
}

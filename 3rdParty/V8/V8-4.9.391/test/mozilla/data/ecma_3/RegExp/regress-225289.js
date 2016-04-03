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
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   PhilSchwartau@aol.com
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
 *
 * Date:    10 November 2003
 * SUMMARY: Testing regexps with complementary alternatives
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=225289
 *
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-225289.js';
var i = 0;
var BUGNUMBER = 225289;
var summary = 'Testing regexps with complementary alternatives';
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


// this pattern should match any string!
pattern = /a|[^a]/;

status = inSection(1);
string = 'a';
actualmatch = string.match(pattern);
expectedmatch = Array('a');
addThis();

status = inSection(2);
string = '';
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();

status = inSection(3);
string = '()';
actualmatch = string.match(pattern);
expectedmatch = Array('(');
addThis();


pattern = /(a|[^a])/;

status = inSection(4);
string = 'a';
actualmatch = string.match(pattern);
expectedmatch = Array('a', 'a');
addThis();

status = inSection(5);
string = '';
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();

status = inSection(6);
string = '()';
actualmatch = string.match(pattern);
expectedmatch = Array('(', '(');
addThis();


pattern = /(a)|([^a])/;

status = inSection(7);
string = 'a';
actualmatch = string.match(pattern);
expectedmatch = Array('a', 'a', undefined);
addThis();

status = inSection(8);
string = '';
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();

status = inSection(9);
string = '()';
actualmatch = string.match(pattern);
expectedmatch = Array('(', undefined, '(');
addThis();


// note this pattern has one non-capturing parens
pattern = /((?:a|[^a])*)/g;

status = inSection(10);
string = 'a';
actualmatch = string.match(pattern);
expectedmatch = Array('a', ''); // see bug 225289 comment 6
addThis();

status = inSection(11);
string = '';
actualmatch = string.match(pattern);
expectedmatch = Array(''); // see bug 225289 comment 9
addThis();

status = inSection(12);
string = '()';
actualmatch = string.match(pattern);
expectedmatch = Array('()', ''); // see bug 225289 comment 6
addThis();




//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------



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

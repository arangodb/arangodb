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
 *   mozilla@pdavis.cx, pschwartau@netscape.com
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
 * Date: 22 October 2001
 *
 * SUMMARY: Regression test for Bugzilla bug 105972:
 * "/^.*?$/ will not match anything"
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=105972
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-105972.js';
var i = 0;
var BUGNUMBER = 105972;
var summary = 'Regression test for Bugzilla bug 105972';
var cnEmptyString = '';
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


/*
 * The bug: this match was coming up null in Rhino and SpiderMonkey.
 * It should match the whole string. The reason:
 *
 * The * operator is greedy, but *? is non-greedy: it will stop
 * at the simplest match it can find. But the pattern here asks us
 * to match till the end of the string. So the simplest match must
 * go all the way out to the end, and *? has no choice but to do it.
 */
status = inSection(1);
pattern = /^.*?$/;
string = 'Hello World';
actualmatch = string.match(pattern);
expectedmatch = Array(string);
addThis();


/*
 * Leave off the '$' condition - here we expect the empty string.
 * Unlike the above pattern, we don't have to match till the end of
 * the string, so the non-greedy operator *? doesn't try to...
 */
status = inSection(2);
pattern = /^.*?/;
string = 'Hello World';
actualmatch = string.match(pattern);
expectedmatch = Array(cnEmptyString);
addThis();


/*
 * Try '$' combined with an 'or' operator.
 *
 * The operator *? will consume the string from left to right,
 * attempting to satisfy the condition (:|$). When it hits ':',
 * the match will stop because the operator *? is non-greedy.
 *
 * The submatch $1 = (:|$) will contain the ':'
 */
status = inSection(3);
pattern = /^.*?(:|$)/;
string = 'Hello: World';
actualmatch = string.match(pattern);
expectedmatch = Array('Hello:', ':');
addThis();


/*
 * Again, '$' combined with an 'or' operator.
 *
 * The operator * will consume the string from left to right,
 * attempting to satisfy the condition (:|$). When it hits ':',
 * the match will not stop since * is greedy. The match will
 * continue until it hits $, the end-of-string boundary.
 *
 * The submatch $1 = (:|$) will contain the empty string
 * conceived to exist at the end-of-string boundary.
 */
status = inSection(4);
pattern = /^.*(:|$)/;
string = 'Hello: World';
actualmatch = string.match(pattern);
expectedmatch = Array(string, cnEmptyString);
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

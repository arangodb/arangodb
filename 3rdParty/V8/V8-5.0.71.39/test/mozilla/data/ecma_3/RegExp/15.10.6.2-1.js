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
 * Date: 23 October 2001
 *
 * SUMMARY: Testing regexps with the global flag set.
 * NOT every substring fitting the given pattern will be matched.
 * The parent string is CONSUMED as successive matches are found.
 *
 * From the ECMA-262 Final spec:
 *
 * 15.10.6.2 RegExp.prototype.exec(string)
 * Performs a regular expression match of string against the regular
 * expression and returns an Array object containing the results of
 * the match, or null if the string did not match.
 *
 * The string ToString(string) is searched for an occurrence of the
 * regular expression pattern as follows:
 *
 * 1.  Let S be the value of ToString(string).
 * 2.  Let length be the length of S.
 * 3.  Let lastIndex be the value of the lastIndex property.
 * 4.  Let i be the value of ToInteger(lastIndex).
 * 5.  If the global property is false, let i = 0.
 * 6.  If i < 0 or i > length then set lastIndex to 0 and return null.
 * 7.  Call [[Match]], giving it the arguments S and i.
 *     If [[Match]] returned failure, go to step 8;
 *     otherwise let r be its State result and go to step 10.
 * 8.  Let i = i+1.
 * 9.  Go to step 6.
 * 10. Let e be r's endIndex value.
 * 11. If the global property is true, set lastIndex to e.
 *
 *          etc.
 *
 *
 * So when the global flag is set, |lastIndex| is incremented every time
 * there is a match; not from i to i+1, but from i to "endIndex" e:
 *
 * e = (index of last input character matched so far by the pattern) + 1
 *
 * Thus in the example below, the first endIndex e occurs after the
 * first match 'a b'. The next match will begin AFTER this, and so
 * will NOT be 'b c', but rather 'c d'. Similarly, 'd e' won't be matched.
 */
//-----------------------------------------------------------------------------
var gTestfile = '15.10.6.2-1.js';
var i = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing regexps with the global flag set';
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
string = 'a b c d e';
pattern = /\w\s\w/g;
actualmatch = string.match(pattern);
expectedmatch = ['a b','c d']; // see above explanation -
addThis();


status = inSection(2);
string = '12345678';
pattern = /\d\d\d/g;
actualmatch = string.match(pattern);
expectedmatch = ['123','456'];
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

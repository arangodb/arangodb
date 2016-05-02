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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Dave Mandelin <dmandelin@mozilla.com>
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

var gTestfile = 'regress-465862.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 465862;
var summary = 'Do case-insensitive matching correctly in JIT for non-ASCII-letters';

var i = 0;
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

// Note: we must call the RegExp constructor here instead of using
// literals. Otherwise, because the regexps are compiled at parse
// time, they will not be compiled to native code and we will not
// actually be testing jitted regexps.

jit(true);

status = inSection(1);
string = '@';
pattern = new RegExp('@', 'i');
actualmatch = string.match(pattern);
expectedmatch = Array(string);
addThis();

status = inSection(2);
string = '`';
pattern = new RegExp('`', 'i');
actualmatch = string.match(pattern);
expectedmatch = Array(string);
addThis();

status = inSection(3);
string = '@';
pattern = new RegExp('`', 'i');
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();

status = inSection(4);
string = '`';
pattern = new RegExp('@', 'i');
print(string + ' ' + pattern);
actualmatch = string.match(pattern);
print('z ' + actualmatch);
print('`'.match(/@/i));
expectedmatch = null;
addThis();

jit(false);

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

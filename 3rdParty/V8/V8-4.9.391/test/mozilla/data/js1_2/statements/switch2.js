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

gTestfile = 'switch2.js';

/**
   Filename:     switch2.js
   Description:  'Tests the switch statement'

   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=323696

   Author:       Norris Boyd
   Date:         July 31, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
var TITLE   = 'statements: switch';
var BUGNUMBER="323626";

startTest();
writeHeaderToLog("Executing script: switch2.js");
writeHeaderToLog( SECTION + " "+ TITLE);

// test defaults not at the end; regression test for a bug that
// nearly made it into 4.06
function f0(i) {
  switch(i) {
  default:
  case "a":
  case "b":
    return "ab*"
      case "c":
    return "c";
  case "d":
    return "d";
  }
  return "";
}
new TestCase(SECTION, 'switch statement',
	     f0("a"), "ab*");

new TestCase(SECTION, 'switch statement',
	     f0("b"), "ab*");

new TestCase(SECTION, 'switch statement',
	     f0("*"), "ab*");

new TestCase(SECTION, 'switch statement',
	     f0("c"), "c");

new TestCase(SECTION, 'switch statement',
	     f0("d"), "d");

function f1(i) {
  switch(i) {
  case "a":
  case "b":
  default:
    return "ab*"
      case "c":
    return "c";
  case "d":
    return "d";
  }
  return "";
}

new TestCase(SECTION, 'switch statement',
	     f1("a"), "ab*");

new TestCase(SECTION, 'switch statement',
	     f1("b"), "ab*");

new TestCase(SECTION, 'switch statement',
	     f1("*"), "ab*");

new TestCase(SECTION, 'switch statement',
	     f1("c"), "c");

new TestCase(SECTION, 'switch statement',
	     f1("d"), "d");

// Switch on integer; will use TABLESWITCH opcode in C engine
function f2(i) {
  switch (i) {
  case 0:
  case 1:
    return 1;
  case 2:
    return 2;
  }
  // with no default, control will fall through
  return 3;
}

new TestCase(SECTION, 'switch statement',
	     f2(0), 1);

new TestCase(SECTION, 'switch statement',
	     f2(1), 1);

new TestCase(SECTION, 'switch statement',
	     f2(2), 2);

new TestCase(SECTION, 'switch statement',
	     f2(3), 3);

// empty switch: make sure expression is evaluated
var se = 0;
switch (se = 1) {
}
new TestCase(SECTION, 'switch statement',
	     se, 1);

// only default
se = 0;
switch (se) {
default:
  se = 1;
}
new TestCase(SECTION, 'switch statement',
	     se, 1);

// in loop, break should only break out of switch
se = 0;
for (var i=0; i < 2; i++) {
  switch (i) {
  case 0:
  case 1:
    break;
  }
  se = 1;
}
new TestCase(SECTION, 'switch statement',
	     se, 1);

// test "fall through"
se = 0;
i = 0;
switch (i) {
case 0:
  se++;
  /* fall through */
case 1:
  se++;
  break;
}
new TestCase(SECTION, 'switch statement',
	     se, 2);
print("hi");

test();

// Needed: tests for evaluation time of case expressions.
// This issue was under debate at ECMA, so postponing for now.


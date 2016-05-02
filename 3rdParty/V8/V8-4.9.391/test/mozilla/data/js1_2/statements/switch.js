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

gTestfile = 'switch.js';

/**
   Filename:     switch.js
   Description:  'Tests the switch statement'

   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=323696

   Author:       Nick Lerissa
   Date:         March 19, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
var TITLE   = 'statements: switch';
var BUGNUMBER="323696";

startTest();
writeHeaderToLog("Executing script: switch.js");
writeHeaderToLog( SECTION + " "+ TITLE);


var var1 = "match string";
var match1 = false;
var match2 = false;
var match3 = false;

switch (var1)
{
case "match string":
  match1 = true;
case "bad string 1":
  match2 = true;
  break;
case "bad string 2":
  match3 = true;
}

new TestCase ( SECTION, 'switch statement',
	       true, match1);

new TestCase ( SECTION, 'switch statement',
	       true, match2);

new TestCase ( SECTION, 'switch statement',
	       false, match3);

var var2 = 3;

var match1 = false;
var match2 = false;
var match3 = false;
var match4 = false;
var match5 = false;

switch (var2)
{
case 1:
/*	        switch (var1)
  {
  case "foo":
  match1 = true;
  break;
  case 3:
  match2 = true;
  break;
  }*/
  match3 = true;
  break;
case 2:
  match4 = true;
  break;
case 3:
  match5 = true;
  break;
}
new TestCase ( SECTION, 'switch statement',
	       false, match1);

new TestCase ( SECTION, 'switch statement',
	       false, match2);

new TestCase ( SECTION, 'switch statement',
	       false, match3);

new TestCase ( SECTION, 'switch statement',
	       false, match4);

new TestCase ( SECTION, 'switch statement',
	       true, match5);

test();

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

gTestfile = 'switch-001.js';

/**
   File Name:          switch-001.js
   Section:
   Description:

   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=315767

   Verify that switches do not use strict equality in
   versions of JavaScript < 1.4.  It's now been decided that
   we won't put in version switches, so all switches will
   be ECMA.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "switch-001";
var VERSION = "JS1_3";
var TITLE   = "switch-001";
var BUGNUMBER="315767";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

result = "fail:  did not enter switch";

switch (true) {
case 1:
  result = "fail:  version 130 should force strict equality";
  break;
case true:
  result = "pass";
  break;
default:
  result = "fail: evaluated default statement";
}

new TestCase(
  SECTION,
  "switch / case should use strict equality in version of JS < 1.4",
  "pass",
  result );

test();


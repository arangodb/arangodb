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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Igor Bukanov <igor@mir2.org>
 *                 Bob Clary <bob@bclary.com>
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

var gTestfile = 'regress-203278-2.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 203278;
var summary = 'Don\'t crash in recursive js_MarkGCThing';
var actual = 'FAIL';
var expect = 'PASS';

printBugNumber(BUGNUMBER);
printStatus (summary);

// Prepare  array to test DeutschSchorrWaite implementation
// and its reverse pointer scanning performance

var a = new Array(1000 * 1000);

var i = a.length;
while (i-- != 0) {
  switch (i % 11) {
  case 0:
    a[i] = { };
    break;
  case 1:
    a[i] = { a: true, b: false, c: 0 };
    break;
  case 2:
    a[i] = { 0: true, 1: {}, 2: false };
    break;
  case 3:
    a[i] = { a: 1.2, b: "", c: [] };
    break;
  case 4:
    a[i] = [ false ];
    break;
  case 6:
    a[i] = [];
    break;
  case 7:
    a[i] = false;
    break;
  case 8:
    a[i] = "x";
    break;
  case 9:
    a[i] = new String("x");
    break;
  case 10:
    a[i] = 1.1;
    break;
  case 10:
    a[i] = new Boolean();
    break;
  }	   
}

printStatus("DSF is prepared");

// Prepare linked list that causes recursion during GC with
// depth O(list size)
// Note: pass "-S 500000" option to the shell to limit stack quota
// available for recursion

for (i = 0; i != 50*1000; ++i) {
  a = [a, a, {}];
  a = [a,  {}, a];

}

printStatus("Linked list is prepared");

gc();

actual = 'PASS';

reportCompare(expect, actual, summary);


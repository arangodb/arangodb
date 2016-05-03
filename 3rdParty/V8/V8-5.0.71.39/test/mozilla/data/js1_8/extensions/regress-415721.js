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
 * Contributor(s): Jason Orendorff <jorendorff@mozilla.com>
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

var gTestfile = 'regress-415721.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 415721;
var summary = 'jsatom.c double hashing re-validation logic is unsound';

printStatus (summary);

var VARS = 10000;
var TRIES = 100;

function atomizeStressTest() {
  var fn = "function f() {var ";
  for (var i = 0; i < VARS; i++)
    fn += '_f' + i + ', ';
  fn += 'q;}';

  function e() { eval(fn); }

  for (var i = 0; i < TRIES; i++) {
    scatter([e,e]);
    gc();
  }
}

var expect;
var actual;

expect = actual = 'No crash';
if (typeof scatter == 'undefined' || typeof gc == 'undefined') {
  print('Test skipped. scatter or gc not defined.');
  expect = actual = 'Test skipped.';
} else {
  atomizeStressTest();
}

reportCompare(expect, actual, summary);


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

var gTestfile = 'for-in.js';
//-----------------------------------------------------------------------------

var summary = "Contention among threads enumerating properties";
// Adapted from mozilla/js/src/xpconnect/tests/js/old/threads.js

printStatus (summary);

var LOOP_COUNT = 1000;
var THREAD_COUNT = 10;

var foo;
var bar;

function makeWorkerFn(id) {
  return function() {
    foo = id + 1;
    bar[id] = {p: 0};
    var n, m;
    for (n in bar) {
      for (m in bar[n]) {}
    }
    for (n in {}.__parent__) {}
  };
}

function range(n) {
  for (let i = 0; i < n; i++)
    yield i;
}

var expect;
var actual;

expect = actual = 'No crash';
if (typeof scatter == 'undefined') {
  print('Test skipped. scatter not defined.');
} else if (!("__parent__" in {})) {
  print('Test skipped. __parent__ not defined.');
} else {
  for (let i = 0; i < LOOP_COUNT; i++) {
    foo = 0;
    bar = new Array(THREAD_COUNT);
    scatter([makeWorkerFn(j) for (j in range(THREAD_COUNT))]);
  }
}

reportCompare(expect, actual, summary);

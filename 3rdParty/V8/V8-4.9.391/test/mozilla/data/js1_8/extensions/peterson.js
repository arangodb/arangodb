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
 * Contributor(s): Guoxin Fan <gfan@sta.samsung.com>
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

var gTestfile = 'peterson.js';
//-----------------------------------------------------------------------

var summary = "Peterson's algorithm for mutual exclusion";

printStatus(summary);

var N = 500;  // number of iterations

// the mutex mechanism
var f = [false, false];
var turn = 0;

// the resource being protected
var counter = 0;

function worker(me) {
  let him = 1 - me;

  for (let i = 0; i < N; i++) {
    // enter the mutex
    f[me] = true;
    turn = him;
    while (f[him] && turn == him)
      ;  // busy wait

    // critical section
    let x = counter;
    sleep(0.003);
    counter = x+1;

    // leave the mutex
    f[me] = false;
  }

  return 'ok';
}

var expect;
var actual;

if (typeof scatter == 'undefined' || typeof sleep == 'undefined') {
  print('Test skipped. scatter or sleep not defined.');
  expect = actual = 'Test skipped.';
} else {
  var results = scatter ([function() { return worker(0); },
                          function() { return worker(1); }]);
  expect = "Thread status: [ok,ok], counter: " + (2 * N);
  actual = "Thread status: [" + results + "], counter: " + counter;
}

reportCompare(expect, actual, summary);

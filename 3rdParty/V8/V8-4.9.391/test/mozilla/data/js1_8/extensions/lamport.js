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
 *                 Jason Orendorff <jorendorff@mozilla.com>
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

var gTestfile = 'lamport.js'
//-----------------------------------------------------------------------

var summary = "Lamport Bakery's algorithm for mutual exclusion";
// Adapted from pseudocode in Wikipedia:
// http://en.wikipedia.org/wiki/Lamport%27s_bakery_algorithm

printStatus(summary);

var N = 15; // Number of threads.
var LOOP_COUNT = 10; // Number of times each thread should loop

function range(n) {
  for (let i = 0; i < n; i++)
    yield i;
}

function max(a) {
  let x = Number.NEGATIVE_INFINITY;
  for each (let i in a)
    if (i > x)
      x = i;
  return x;
}

// the mutex mechanism
var entering = [false for (i in range(N))];
var ticket = [0 for (i in range(N))];

// the object being protected
var counter = 0;

function lock(i)
{
  entering[i] = true;
  ticket[i] = 1 + max(ticket);
  entering[i] = false;

  for (let j = 0; j < N; j++) {
    // If thread j is in the middle of getting a ticket, wait for that to
    // finish.
    while (entering[j])
      ;

    // Wait until all threads with smaller ticket numbers or with the same
    // ticket number, but with higher priority, finish their work.
    while ((ticket[j] != 0) && ((ticket[j] < ticket[i]) ||
                                ((ticket[j] == ticket[i]) && (i < j))))
      ;
  }
}

function unlock(i) {
  ticket[i] = 0;
}

function worker(i) {
  for (let k = 0; k < LOOP_COUNT; k++) {
    lock(i);

    // The critical section
    let x = counter;
    sleep(0.003);
    counter = x + 1;

    unlock(i);
  }
  return 'ok';
}

function makeWorker(id) {
  return function () { return worker(id); };
}

var expect;
var actual;

if (typeof scatter == 'undefined' || typeof sleep == 'undefined') {
  print('Test skipped. scatter or sleep not defined.');
  expect = actual = 'Test skipped.';
} else {
  scatter([makeWorker(i) for (i in range(N))]);

  expect = "counter: " + (N * LOOP_COUNT);
  actual = "counter: " + counter;
}

reportCompare(expect, actual, summary);

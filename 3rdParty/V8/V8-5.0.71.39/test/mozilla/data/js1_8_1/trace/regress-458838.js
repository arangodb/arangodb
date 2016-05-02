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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Boris Zbarksy
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

var gTestfile = 'regress-458838.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 458838;
var summary = 'TM: do not fall off trace when nested function accesses var of outer function';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  jit(true);

  function f() {
    var a = 1;
    function g() {
      var b = 0
        for (var i = 0; i < 10; ++i) {
          b += a;
        }
      return b;
    }

    return g();
  }

  expect = 10;
  actual  = f();

  var recorderStarted;
  var recorderAborted;
  var traceCompleted;

  if (this.tracemonkey)
  {
    recorderStarted = this.tracemonkey.recorderStarted;
    recorderAborted = this.tracemonkey.recorderAborted;
    traceCompleted  = this.tracemonkey.traceCompleted;
  }

  jit(false);

  reportCompare(expect, actual, summary + ': return value 10');

  if (this.tracemonkey)
  {
    expect = 'recorderStarted=1, recorderAborted=0, traceCompleted=1';
    actual = 'recorderStarted=' + recorderStarted + ', recorderAborted=' + recorderAborted + ', traceCompleted=' + traceCompleted;
    reportCompare(expect, actual, summary + ': trace');
  }

  exitFunc ('test');
}

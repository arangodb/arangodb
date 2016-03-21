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
 * Jeff Walden.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

var gTestfile = 'fibonacci-matrix-generator.js';
//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "Fibonacci generator by matrix multiplication";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function fib()
{
  var init = [1, 0];
  var mx = [[1, 1], [1, 0]];
  while (true)
  {
    yield init[1];
    var tmp = [,];
    tmp[0] =
      mx[0][0]*init[0] + mx[0][1]*init[1];
    tmp[1] =
      mx[1][0]*init[0] + mx[1][1]*init[1];
    init = tmp;
  }
}

var failed = false;
var it = fib();

try
{
  if (it.next() != 0)
    throw "F_0 failed";
  if (it.next() != 1)
    throw "F_1 failed";
  if (it.next() != 1)
    throw "F_2 failed";
  if (it.next() != 2)
    throw "F_3 failed";
  if (it.next() != 3)
    throw "F_4 failed";
  if (it.next() != 5)
    throw "F_5 failed";
  if (it.next() != 8)
    throw "F_6 failed";
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);

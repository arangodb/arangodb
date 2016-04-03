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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Julien Lecomte
 *                 Igor Bukanov
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

var gTestfile = 'regress-313967-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 313967;
var summary = 'Compile time of N functions should be O(N)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function F(x, y) {
  var a = x+y;
  var b = x-y;
  var c = a+b;
  return ( 2*c-1 );
}

function test(N)
{
  var src = F.toSource(-1)+"\n";
  src = Array(N + 1).join(src);
  var counter = 0;
  src = src.replace(/F/g, function() { ++counter; return "F"+counter; })
    var now = Date.now;
  var t = now();
  eval(src);
  return now() - t;
}


var times = [];
for (var i = 0; i != 10; ++i) {
  var time = test(1000 + i * 2000);
  times.push(time);
  gc();
}

var r = calculate_least_square_r(times);

print('test times: ' + times);

print("r coefficient for the estimation that eval is O(N) where N is number\n"+
      "of functions in the script: "+r);

print( r < 0.97 ? "Eval time growth is not linear" : "The growth seems to be linear");

reportCompare(true, r >= 0.97, summary);

function calculate_least_square_r(array)
{
  var sum_x = 0;
  var sum_y = 0;
  var sum_x_squared = 0;
  var sum_y_squared = 0;
  var sum_xy = 0;
  var n  = array.length;
  for (var i = 0; i != n; ++i) {
    var x = i;
    var y = array[i];
    sum_x += x;
    sum_y += y;
    sum_x_squared += x*x;
    sum_y_squared += y*y;
    sum_xy += x*y;
  }
  var x_average = sum_x / n;
  var y_average = sum_y / n;
  var sxx = sum_x_squared - n * x_average * x_average;
  var syy = sum_y_squared - n * y_average * y_average;
  var sxy = sum_xy - n * x_average * y_average;
  var r = sxy*sxy/(sxx*syy);
  return r;
}
 

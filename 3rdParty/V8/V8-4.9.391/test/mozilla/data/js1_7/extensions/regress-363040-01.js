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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Brendan Eich
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

var gTestfile = 'regress-363040-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 363040;
var summary = 'Array.prototype.reduce application in continued fraction';
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
 
// Print x as a continued fraction in compact abbreviated notation and return
// the convergent [n, d] such that x - (n / d) <= epsilon.
  function contfrac(x, epsilon) {
    let i = Math.floor(x);
    let a = [i];
    x = x - i;
    let maxerr = x;
    while (maxerr > epsilon) {
      x = 1 / x;
      i = Math.floor(x);
      a.push(i);
      x = x - i;
      maxerr = x * maxerr / i;
    }
    print(uneval(a));
    a.push([1, 0]);
    a.reverse();
    return a.reduce(function (x, y) {return [x[0] * y + x[1], x[0]];});
  }

  if (!Array.prototype.reduce)
  {
    print('Test skipped. Array.prototype.reduce not implemented');
  }
  else
  {
// Show contfrac in action.
    for each (num in [Math.PI, Math.sqrt(2), 1 / (Math.sqrt(Math.E) - 1)]) {
      print('Continued fractions for', num);
      for each (eps in [1e-2, 1e-3, 1e-5, 1e-7, 1e-10]) {
        let frac = contfrac(num, eps);
        let est = frac[0] / frac[1];
        let err = num - est;
        print(uneval(frac), est, err);
      }
      print();
    }
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

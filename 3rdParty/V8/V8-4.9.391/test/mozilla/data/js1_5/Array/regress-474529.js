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
 * Contributor(s): Biju
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

var gTestfile = 'regress-474529.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 474529;
var summary = 'Do not assert: _buf->_nextPage';
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

  function main() {

    function timeit(N, buildArrayString) {
      return Function("N",
                      "var d1 = +new Date;" +
                      "while (N--) var x = " + buildArrayString +
                      "; return +new Date - d1")(N);
    }

    function reportResults(size, N, literalMs, newArrayMs, arrayMs) {
      print(Array.join(arguments, "\t"));
    }

    var repetitions = [ 9000, 7000, 4000, 2000, 2000, 2000, 800, 800, 800, 300, 100, 100 ]
      for (var zeros = "0, ", i = 0; i < repetitions.length; ++i) {
        var N = repetitions[i];
        reportResults((1 << i) + 1, N,
                      timeit(N, "[" + zeros + " 0 ]"),
                      timeit(N, "new Array(" + zeros + " 0)"),
                      timeit(N, "Array(" + zeros + " 0)"));
        zeros += zeros;
      }
  }

  jit(true);
  gc();
  print("Size\t\Rep.\t\Literal\tnew Arr\tArray()");
  print("====\t=====\t=======\t=======\t=======");
  main();
  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

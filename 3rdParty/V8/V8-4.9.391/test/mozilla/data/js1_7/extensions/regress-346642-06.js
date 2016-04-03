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
 * Contributor(s): Jesse Ruderman
 *                 Brendan Eich
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
 * the provisions above, a recipient may use your version of this file under    let (x=3) ((++x)())

 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gTestfile = 'regress-346642-06.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 346642;
var summary = 'decompilation of destructuring assignment';
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

  expect = 3;
  actual = '';
  "" + function() { [] = 3 }; actual = 3;
  actual = 3;
  reportCompare(expect, actual, summary + ': 1');

  try
  {
    var z = 6;
    var f = eval('(function (){for(let [] = []; false;) let z; return z})');
    expect =  f();
    actual = eval("("+f+")")()
      reportCompare(expect, actual, summary + ': 2');
  }
  catch(ex)
  {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=408957
    var summarytrunk = 'let declaration must be direct child of block or top-level implicit block';
    expect = 'SyntaxError';
    actual = ex.name;
    reportCompare(expect, actual, summarytrunk);
  }

  expect = 3;
  actual = '';
  "" + function () { for(;; [[a]] = [5]) { } }; actual = 3;
  reportCompare(expect, actual, summary + ': 3');

  expect = 3;
  actual = '';
  f = function () { return { set x([a]) { yield; } } }
  var obj = f();
  uneval(obj); actual = 3;
  reportCompare(expect, actual, summary + ': 4');

  expect = 3;
  actual = '';
  "" + function () { [y([a]=b)] = z }; actual = 3;
  reportCompare(expect, actual, summary + ': 5');

  expect = 3;
  actual = '';
  "" + function () { for(;; ([[,]] = p)) { } }; actual = 3;
  reportCompare(expect, actual, summary + ': 6');

  expect = 3;
  actual = '';
  actual = 1; try {for(x in (function ([y]) { })() ) { }}catch(ex){} actual = 3;
  reportCompare(expect, actual, summary + ': 7');

  exitFunc ('test');
}

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
 * Contributor(s): Jesse Ruderman
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

var gTestfile = 'regress-380237-03.js';

//-----------------------------------------------------------------------------
var BUGNUMBER = 380237;
var summary = 'Decompilation of generator expressions';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

expect = 'No Error';
actual = '';
try
{
  var g = ((yield i) for (i in [1,2,3]));
  actual = 'No Error';
}
catch(ex)
{
  actual = ex + '';
}
reportCompare(expect, actual, summary + ': top level');


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var f = (function() { g = (d for (d in [0]) for (e in [1])); });
  expect = 'function() { g = (d for (d in [0]) for (e in [1])); }';
  actual = f + '';
  compareSource(expect, actual, summary + ': see bug 380506');

  f = function() { return (1 for (i in [])) };
  expect = 'function() { return (1 for (i in [])); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { with((x for (x in []))) { } };
  expect = 'function() { with(x for (x in [])) { } }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = (function() { (1 for (w in []) if (0)) });
  expect = 'function() { (1 for (w in []) if (0)); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = (function() { (1 for (w in []) if (x)) });
  expect = 'function() { (1 for (w in []) if (x)); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = (function() { (1 for (w in []) if (1)) });
  expect = 'function() { (1 for (w in []) ); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = (function() { (x for ([{}, {}] in [])); });
  expect = 'function() { (x for ([[], []] in [])); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  expect = 'SyntaxError: invalid assignment left-hand side';
  actual = '';
  try
  {
    eval('(function() { (x for each (x in [])) = 3; })');
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': Do not Assert: *pc == JSOP_CALL');

  f = (function() { (x*x for (x in a)); });
  expect = 'function() { (x*x for (x in a)); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = (function () { (1 for (y in (yield 3))); });
  expect = 'function () { (1 for (y in yield 3)); }';
  actual = f + '';
  compareSource(expect, actual, summary);
   
  expect = 'SyntaxError: invalid delete operand';
  try
  {
    eval('(function () { delete (x for (x in [])); })');
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': Do not Assert: *pc == JSOP_CALL');

  f = (function() { ([yield] for (x in [])); });
  expect = 'function() { ([yield] for (x in [])); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { if(1, (x for (x in []))) { } };
  expect = 'function() { if(1, (x for (x in []))) { } }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function () {return(i*j for each (i in [1,2,3,4]) 
                          for each (j in [5,6,7,8]))};
  expect = 'function () {return(i*j for each (i in [1,2,3,4]) ' + 
    'for each (j in [5,6,7,8]));}';
  actual = f + '';
  compareSource(expect, actual, summary);

  expect = 'No Error';
  actual = '';
  try
  {
    var g = ((yield i) for (i in [1,2,3]));
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': nested');

  f = function() { try { } catch(x if (1 for (x in []))) { } finally { } };
  expect = 'function() { try {} catch(x if (1 for (x in []))) {} finally {} }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = eval(uneval(f));
  expect = 'function() { try {} catch(x if (1 for (x in []))) {} finally {} }';
  actual = f + '';
  compareSource(expect, actual, summary + ': eval(uneval())');

  f = function() { if (1, (x for (x in []))) { } };
  expect = 'function() { if (1, (x for (x in []))) { } }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { ((a, b) for (x in [])) };
  expect = 'function() { ((a, b) for (x in [])); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = (function() { ({x setter: (function () {}).x }) });
  expect = 'function() { ({x setter: function () {}.x }); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}

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
 * Contributor(s): Robert Sayre
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

var gTestfile = 'regress-366941.js';

//-----------------------------------------------------------------------------
var BUGNUMBER = 366941;
var summary = 'Destructuring enumerations, iterations';
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

  var list1 = [[1,2],[3,4],[5,6]];
  var list2 = [[1,2,3],[4,5,6],[7,8,9]];

  expect = '1,2;3,4;5,6;';
  actual = '';

  for each (var [foo, bar] in list1) {
    actual += foo + "," + bar + ";";
  }

  reportCompare(expect, actual, summary + ': 1');

  expect = '1,2,3;4,5,6;7,8,9;';
  actual = '';
  for each (var [foo, bar, baz] in list2) {
    actual += foo + "," + bar + "," + baz + ";";
  }

  reportCompare(expect, actual, summary + ': 2');

  function gen(list) {
    for each (var test in list) {
      yield test;
    }
  }

  var iter1 = gen(list1);

  expect = '1,2;3,4;5,6;';
  actual = '';

  for (var [foo, bar] in iter1) {
    actual += foo + "," + bar + ";";
  }

  reportCompare(expect, actual, summary + ': 3');

  var iter2 = gen(list2);
  expect = '1,2,3;4,5,6;7,8,9;';
  actual = '';

  for (var [foo, bar, baz] in iter2) {
    actual += foo + "," + bar + "," + baz + ";";
  }

  reportCompare(expect, actual, summary + ': 4');

  exitFunc ('test');
}

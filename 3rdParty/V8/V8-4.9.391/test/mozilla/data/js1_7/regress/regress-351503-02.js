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

var gTestfile = 'regress-351503-02.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 351503;
var summary = 'decompilation of TypeError messages';
var actual = '';
var expect = '';

test();

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = 'TypeError: can\'t convert Object to string';
  try
  {
    function p() { return { toString: null } } "fafa".replace(/a/g, p);
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 1');

  expect = 'TypeError: can\'t convert Object to string';
  try
  {
    a=1; b=2; c={toString: null}; "hahbhc".replace(/[abc]/g, eval);
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 2');

  expect = 'TypeError: can\'t convert ({toString:(function () {yield 4;})}) to primitive type';
  try
  {
    3 + ({toString:function(){yield 4} });
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 3');

  expect = 'TypeError: ({}) is not a function';
  try
  {
    3 + ({toString:({}) }) ;
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 4');

  expect = 'TypeError: 3 is not a function';
  try
  {
    ([5].map)(3);
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 5');

  expect = 'TypeError: 5 is not a function';
  try
  {
    (1 + (4))();
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 6');
}

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
 * Contributor(s): nanto_vi (TOYAMA Nao)
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

var gTestfile = '15.9.4.3.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 363578;
var summary = '15.9.4.3 - Date.UTC edge-case arguments.';
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
 
  //

  expect = 31;
  actual = (new Date(Date.UTC(2006, 0, 0)).getUTCDate());
  reportCompare(expect, actual, summary + ': date 0');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours 0');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes 0');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0, 0)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds 0');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0, 0, 0)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds 0');

  //

  expect = 30;
  actual = (new Date(Date.UTC(2006, 0, -1)).getUTCDate());
  reportCompare(expect, actual, summary + ': date -1');

  expect = 23;
  actual = (new Date(Date.UTC(2006, 0, 0, -1)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours -1');

  expect = 59;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, -1)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes -1');

  expect = 59;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0, -1)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds -1');

  expect = 999;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0, 0, -1)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds -1');

  //

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, undefined)).getUTCDate());
  reportCompare(expect, actual, summary + ': date undefined');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, undefined)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours undefined');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, undefined)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes undefined');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, undefined)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds undefined');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, 0, undefined)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds undefined');

  //

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, {})).getUTCDate());
  reportCompare(expect, actual, summary + ': date {}');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, {})).getUTCHours());
  reportCompare(expect, actual, summary + ': hours {}');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, {})).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes {}');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, {})).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds {}');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, 0, {})).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds {}');

  //

  expect = 31;
  actual = (new Date(Date.UTC(2006, 0, null)).getUTCDate());
  reportCompare(expect, actual, summary + ': date null');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, null)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours null');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, null)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes null');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0, null)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds null');

  expect = 0;
  actual = (new Date(Date.UTC(2006, 0, 0, 0, 0, 0, null)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds null');

  //

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, Infinity)).getUTCDate());
  reportCompare(expect, actual, summary + ': date Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, Infinity)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, Infinity)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, Infinity)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, 0, Infinity)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds Infinity');

  //

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, -Infinity)).getUTCDate());
  reportCompare(expect, actual, summary + ': date -Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, -Infinity)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours -Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, -Infinity)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes -Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, -Infinity)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds -Infinity');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, 0, -Infinity)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds -Infinity');

  //

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, NaN)).getUTCDate());
  reportCompare(expect, actual, summary + ': date NaN');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, NaN)).getUTCHours());
  reportCompare(expect, actual, summary + ': hours NaN');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, NaN)).getUTCMinutes());
  reportCompare(expect, actual, summary + ': minutes NaN');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, NaN)).getUTCSeconds());
  reportCompare(expect, actual, summary + ': seconds NaN');

  expect = true;
  actual = isNaN(new Date(Date.UTC(2006, 0, 0, 0, 0, 0, NaN)).getUTCMilliseconds());
  reportCompare(expect, actual, summary + ': milliseconds NaN');

  exitFunc ('test');
}

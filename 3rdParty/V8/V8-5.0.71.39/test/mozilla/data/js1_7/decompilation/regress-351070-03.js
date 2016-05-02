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

var gTestfile = 'regress-351070-03.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 351070;
var summary = 'decompilation of let declaration should not change scope';
var summarytrunk = 'let declaration must be direct child of block or top-level implicit block';
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

  var c;
  var f;

  try
  {
    c = '(function (x){if (x) if (y) z; else let w })';
    f = eval(c);
    expect = 'function (x){if (x) {if (y) { z; } else let w; }}';
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=408957
    expect = 'SyntaxError';
    actual = ex.name;
    reportCompare(expect, actual, summarytrunk + ': ' + c);
  }

  try
  {
    c = '(function (x){if (x){ if (y) z;} else let w  })';
    f = eval(c);
    expect = 'function (x){if (x){ if (y) {z;}} else let w;  }';
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=408957
    expect = 'SyntaxError';
    actual = ex.name;
    reportCompare(expect, actual, summarytrunk + ': ' + c);
  }

  try
  {
    c = '(function (x){if (x){ if (y) let z;} else let w  })';
    f = eval(c);
    expect = 'function (x){if (x){ if (y) let z;} else let w;  }';
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=408957
    expect = 'SyntaxError';
    actual = ex.name;
    reportCompare(expect, actual, summarytrunk + ': ' + c);
  }

  try
  {
    c = '(function f(){var a = 2; if (x) {let a = 3; print(a)} return a})';
    f = eval(c);
    expect = 'function f(){var a = 2; if (x) {let a = 3; print(a);} return a;}';
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=408957
    expect = 'SyntaxError';
    actual = ex.name;
    reportCompare(expect, actual, summarytrunk + ': ' + c);
  }

  try
  {
    c = '(function f(){var a = 2; if (x) {print(a);let a = 3} return a})';
    f = eval(c);
    expect = 'function f(){var a = 2; if (x) {print(a);let a = 3;} return a;}';
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=408957
    expect = 'SyntaxError';
    actual = ex.name;
    reportCompare(expect, actual, summarytrunk + ': ' + c);
  }

  try
  {
    c = '(function f(){var a = 2; if (x) {let a = 3} return a})';
    f = eval(c);
    expect = 'function f(){var a = 2; if (x) {let a = 3;} return a;}';
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=408957
    expect = 'SyntaxError';
    actual = ex.name;
    reportCompare(expect, actual, summarytrunk + ': ' + c);
  }

  try
  {
    c = '(function f(){var a = 2; if (x) let a = 3; return a})';
    f = eval(c);
    expect = 'function f(){var a = 2; if (x) let a = 3; return a;}';
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=408957
    expect = 'SyntaxError';
    actual = ex.name;
    reportCompare(expect, actual, summarytrunk + ': ' + c);
  }

  exitFunc ('test');
}

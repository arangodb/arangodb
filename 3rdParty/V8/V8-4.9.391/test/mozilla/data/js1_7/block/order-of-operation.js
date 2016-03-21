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
 * Jeff Walden <jwalden+code@mit.edu>.
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

var gTestfile = 'order-of-operation.js';
//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "Test let and order of operation issues";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function f1(x)
{
  // scope of lhs x includes rhs, so x is NaN here -- bug 344952
  let x = ++x;
  return x;
}

function f2(x)
{
  // scope of lhs x includes rhs, so x is NaN here -- bug 344952
  let x = x++;
  return x;
}

function f3(x)
{
  var q = x;
  let (x = x++)
  {
    if (x != q)
      throw "f3():\n" +
	"  expected: x == q\n" +
	"  actual:   x != q " +
	"(where x == " + x + ", q == " + q + ")\n";
  }
  return x;
}

function f4(x)
{
  var y = 7;
  let (y = x, x = 3)
  {
    var q = 7 + x;
  }
  return x + y + q;
}

function f5(x)
{
  var q = x++;
  let (y = x, r = 17, m = 32) {
    return function(code)
    {
      return eval(code);
    };
  }
}

function f6() {
  var i=3;
  for (let i=i;;) { if (i != 3) throw "fail"; i = 7; break; }
  if (i != 3) throw "fail";
}

try
{
  var rv = f1(5);
  if (!isNaN(rv))
    throw "f1(5):\n" +
      "  expected:  NaN\n" +
      "  actual:    " + rv;

  rv = f2(5);
  if (!isNaN(rv))
    throw "f2(5):\n" +
      "  expected:  NaN\n" +
      "  actual:    " + rv;

  rv = f3(8);
  if (rv != 9)
    throw "f3(8):\n" +
      "  expected:  9\n" +
      "  actual:    " + rv;

  rv = f4(13);
  if (rv != 30)
    throw "f4(13):\n" +
      "  expected:  30\n" +
      "  actual:    " + rv;

  var fun = f5(2);

  rv = fun("q");
  if (rv != 2)
    throw "fun('q'):\n" +
      "  expected:  2\n" +
      "  actual:    " + rv;

  rv = fun("x");
  if (rv != 3)
    throw "fun('x'):\n" +
      "  expected:  3\n" +
      "  actual:    " + rv;

  rv = fun("y");
  if (rv != 3)
    throw "fun('y'):\n" +
      "  expected:  3\n" +
      "  actual:    " + rv;

  rv = fun("let (y = y) { y += 32; }; y");
  if (rv != 3)
    throw "fun('let (y = y) { y += 32; }; y'):\n" +
      "  expected:  3\n" +
      "  actual:    " + rv;

  f6();
}
catch (e)
{
  failed = e;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);

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

var gTestfile = 'regress-344139.js';
//-----------------------------------------------------------------------------
var BUGNUMBER     = "344139";
var summary = "Basic let functionality";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

var x = 7;

function f1()
{
  let x = 5;
  return x;
}

function f2()
{
  let x = 5;
  x = 3;
  return x;
}

function f3()
{
  let x = 5;
  x += x;
  return x;
}

function f4()
{
  var v = 5;
  let q = 17;

  // 0+1+2+...+8+9 == 45
  for (let v = 0; v < 10; v++)
    q += v;

  if (q != 62)
    throw "f4(): wrong value for q\n" +
      "  expected: 62\n" +
      "  actual:   " + q;

  return v;
}

try
{
  if (f1() != 5 || x != 7)
    throw "f1() == 5";
  if (f2() != 3 || x != 7)
    throw "f2() == 3";
  if (f3() != 10 || x != 7)
    throw "f3() == 10";

  if (f4() != 5)
    throw "f4() == 5";

  var bad = true;
  try
  {
    eval("q++"); // force error at runtime
  }
  catch (e)
  {
    if (e instanceof ReferenceError)
      bad = false;
  }
  if (bad)
    throw "f4(): q escaping scope!";
}
catch (e)
{
  failed = e;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);

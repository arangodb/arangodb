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

var gTestfile = 'destructuring-order.js';
//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "Order of destructuring, destructuring in the presence of " +
  "exceptions";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;


var a = "FAILED", b = "PASSED";

function exceptObj()
{
  return { get b() { throw "PASSED"; }, a: "PASSED" };
}

function partialEvalObj()
{
  try
  {
    ({a:a, b:b}) = exceptObj();
    throw "FAILED";
  }
  catch (ex)
  {
    if (ex !== "PASSED")
      throw "bad exception thrown: " + ex;
  }
}


var c = "FAILED", d = "FAILED", e = "PASSED", f = "PASSED";

function exceptArr()
{
  return ["PASSED", {e: "PASSED", get f() { throw "PASSED"; }}, "FAILED"];
}

function partialEvalArr()
{
  try
  {
    [c, {e: d, f: e}, f] = exceptArr();
    throw "FAILED";
  }
  catch (ex)
  {
    if (ex !== "PASSED")
      throw "bad exception thrown: " + ex;
  }
}


var g = "FAILED", h = "FAILED", i = "FAILED", j = "FAILED", k = "FAILED";
var _g = "PASSED", _h = "FAILED", _i = "FAILED", _j = "FAILED", _k = "FAILED";
var order = [];

function objWithGetters()
{
  return {
    get j()
    {
      var rv = _j;
      _g = _h = _i = _j = "FAILED";
      _k = "PASSED";
      order.push("j");
      return rv;
    },
      get g()
      {
	var rv = _g;
	_g = _i = _j = _k = "FAILED";
	_h = "PASSED";
	order.push("g");
	return rv;
      },
	get i()
	{
	  var rv = _i;
	  _g = _h = _i = _k = "FAILED";
	  _j = "PASSED";
	  order.push("i");
	  return rv;
	},
	  get k()
	  {
	    var rv = _k;
	    _g = _h = _i = _j = _k = "FAILED";
	    order.push("k");
	    return rv;
	  },
	    get h()
	    {
	      var rv = _h;
	      _g = _h = _j = _k = "FAILED";
	      _i = "PASSED";
	      order.push("h");
	      return rv;
	    }
  };
}

function partialEvalObj2()
{
  ({g: g, h: h, i: i, j: j, k: k}) = objWithGetters();
}

try
{
  partialEvalObj();
  if (a !== "PASSED" || b !== "PASSED")
    throw "FAILED: lhs not mutated correctly during destructuring!\n" +
      "a == " + a + ", b == " + b;

  partialEvalObj2();
  if (g !== "PASSED" ||
      h !== "PASSED" ||
      i !== "PASSED" ||
      j !== "PASSED" ||
      k !== "PASSED")
    throw "FAILED: order of property accesses wrong!\n" +
      "order == " + order;

  partialEvalArr();
  if (c !== "PASSED" || d !== "PASSED" || e !== "PASSED")
    throw "FAILED: lhs not mutated correctly during destructuring!\n" +
      "c == " + c +
      ", d == " + d +
      ", e == " + e +
      ", f == " + f ;
}
catch (ex)
{
  failed = ex;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);

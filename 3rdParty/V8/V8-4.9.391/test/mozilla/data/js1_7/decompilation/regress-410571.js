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
 * Portions created by the Initial Developer are Copyright (C) 2008
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

var gTestfile = 'regress-410571.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 410571;
var summary = 'incorrect decompilation of last element of object literals';
var actual, expect;

actual =  expect = 'PASSED';

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function getProps(o)
{
  var props = [];
  for (var i in o)
    props.push(i);
  return props.sort().join(",");
}

var tests =
  [
   {
    fun: function()
    {
      yield { x: 1, y: let (z = 7) z };
    },
    generates:
      [
       function(rv)
       {
         return typeof rv === "object" &&
                rv.x === 1 &&
                rv.y === 7 &&
                getProps(rv) === "x,y";
       },
      ]
   },
   {
    fun: function()
     {
       var t = { x: 3, y: yield 4 };
       yield t;
     },
     generates:
       [
        function(rv) { return rv == 4; },
        function(rv)
        {
          return typeof rv === "object" &&
                 rv.x === 3 &&
                 rv.y === undefined &&
                 getProps(rv) === "x,y";
        },
       ]
   },
   {
    fun: function()
    {
      var t = { x: 3, get y() { return 17; } };
      yield t;
    },
    generates:
      [
       function(rv)
       {
         return typeof rv === "object" &&
                rv.x === 3 &&
                rv.y === 17 &&
                getProps(rv) === "x,y";
       }
      ]
   },
   {
    fun: function()
    {
      function q() { return 32; }
      var x = { x getter: q };
      yield x;
    },
    generates:
      [
       function(rv)
       {
         return typeof rv === "object" &&
                getProps(rv) === "x" &&
                rv.x === 32;
       }
      ]
   },
  ];

function checkItems(name, gen)
{
  var i = 0;
  for (var item in gen)
  {
    if (!test.generates[i](item))
      actual = "wrong generated value (" + item + ") " +
            "for test " + name + ", item " + i;
    i++;
  }
  if (i !== test.generates.length)
    actual = "Didn't iterate all of test " + name;
}

for (var i = 0, sz = tests.length; i < sz; i++)
{
  var test = tests[i];

  var fun = test.fun;
  checkItems(i, fun());

  var dec = '(' + fun.toString() + ')';
  var rec = eval(dec);
  checkItems("recompiled " + i, rec());
}

reportCompare(expect, actual, summary);

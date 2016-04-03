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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Igor Bukanov
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

var gTestfile = 'regress-417131.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 417131;
var summary = 'stress test for cache';
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

  function f(N)
  {
    for (var i = 0; i != N; ++i) {
      var obj0 = {}, obj1 = {}, obj2 = {};
      obj1['a'+i] = 0;
      obj2['b'+i] = 0;
      obj2['b'+(i+1)] = 1;
      for (var repeat = 0;repeat != 2; ++repeat) {
        var count = 0;
        for (var j in obj1) {
          if (j !== 'a'+i)
            throw "Bad:"+j;
          for (var k in obj2) {
            if (i == Math.floor(N/3) || i == Math.floor(2*N/3))
              gc();
            var expected;
            switch (count) {
            case 0: expected='b'+i; break;
            case 1: expected='b'+(i+1); break;
            default:
              throw "Bad count: "+count;
            }
            if (expected != k)
              throw "Bad k, expected="+expected+", actual="+k;
            for (var l in obj0)
              ++count;
            ++count;
          }
        }
        if (count !== 2)
          throw "Bad count: "+count;
      }
    }
  }

  var array = [function() { f(10); },
               function() { f(50); },
               function() { f(200); },
               function() { f(400); }
    ];

  if (typeof scatter  == "function") {
    scatter(array);
  } else {
    for (var i = 0; i != array.length; ++i)
      array[i]();
  }

 
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

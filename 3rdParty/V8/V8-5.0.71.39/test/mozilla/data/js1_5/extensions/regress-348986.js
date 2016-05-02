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

var gTestfile = 'regress-348986.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 348986;
var summary = 'Recursion check of nested functions';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
// Construct f(){function f(){function f(){...}}} with maximum
// nested function declaration still does not hit recursion limit.

  var deepestFunction;

  var n = findActionMax(function(n) {
			  var prefix="function f(){";
			  var suffix="}";
			  var source = Array(n+1).join(prefix) + Array(n+1).join(suffix);
			  try {
			    deepestFunction = Function(source);
			    return true;
			  } catch (e) {
			    if (!(e instanceof InternalError))
			      throw e;
			    return false;
			  }

			});

  if (n == 0)
    throw "unexpected";

  print("Max nested function leveles:"+n);

  n = findActionMax(function(n) {
		      try {
			callAfterConsumingCStack(n, function() {});
			return true;
		      } catch (e) {
			if (!(e instanceof InternalError))
			  throw e;
			return false;
		      }
		    });

  print("Max callAfterConsumingCStack levels:"+n);

// Here n is max possible value when callAfterConsumingCStack(n, emptyFunction)
// does not trigger stackOverflow. Decrease it slightly to give some C stack
// space for deepestFunction.toSource()
 
  n = Math.max(0, n - 10);
  try {
    var src = callAfterConsumingCStack(n, function() {
					 return deepestFunction.toSource();
				       });
    throw "Test failed to hit the recursion limit.";
  } catch (e) {
    if (!(e instanceof InternalError))
      throw e;
  }

  print('Done');
  expect = true;
  actual = true;
  reportCompare(expect, true, summary);

  exitFunc ('test');
}

function callAfterConsumingCStack(n, action)
{
  var testObj = { 
    get propertyWithGetter() {
      if (n == 0)
	return action();
      n--;
      return this.propertyWithGetter;
    }
  };
  return testObj.propertyWithGetter;
}


// Return the maximum positive value of N where action(N) still returns true
// or 0 if no such value exists.
function findActionMax(action)
{
  var N, next, increase;

  n = 0;
  for (;;) {
    var next = (n == 0 ? 1 : n * 2);
    if (!isFinite(next) || !action(next))
      break;
    n = next;
  }
  if (n == 0)
    return 0;
	
  var increase = n / 2;
  for (;;) {
    var next = n + increase;
    if (next == n)
      break;
    if (isFinite(next) && action(next)) {
      n = next;
    } else if (increase == 1) {
      break;
    } else {
      increase = increase / 2;
    }	
  }
  return n;
}

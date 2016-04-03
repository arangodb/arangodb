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

var gTestfile = 'regress-396684.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 396684;
var summary = 'Function call with stack arena exhausted';
var actual = '';
var expect = '';

/*
  The test case builds a function containing f(0,0,...,0,Math.atan2()) with
  enough zero arguments to exhaust the current stack arena fully. Thus, when
  Math.atan2 is loaded into the stack, there would be no room for the extra 2
  args required by Math.atan2 and args will be allocated from the new arena.
*/


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function f() {
  return arguments[arguments.length - 1];
}

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = 'PASS';

  var src = "return f(" +Array(10*1000).join("0,")+"Math.atan2());";

  var result = new Function(src)();

  if (typeof result != "number" || !isNaN(result))
    actual = "unexpected result: " + result;
  else
    actual = 'PASS';

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

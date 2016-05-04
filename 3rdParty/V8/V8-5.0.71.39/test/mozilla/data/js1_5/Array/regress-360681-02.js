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

var gTestfile = 'regress-360681-02.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 360681;
var summary = 'Regression from bug 224128';
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
 
  expect = actual = 'No Crash';

  var N = 1000;

// Make an array with a hole at the end
  var a = Array(N);
  for (i = 0; i < N - 1; ++i)
    a[i] = 1;

// array_sort due for array with N elements with allocates a temporary vector
// with 2*N. Lets create strings that on 32 and 64 bit CPU cause allocation
// of the same amount of memory + 1 word for their char arrays. After we GC
// strings with a reasonable malloc implementation that memory will be most
// likely reused in array_sort for the temporary vector. Then the bug causes
// accessing the one-beyond-the-aloocation word and re-interpretation of
// 0xFFF0FFF0 as GC thing.

  var str1 = Array(2*(2*N + 1) + 1).join(String.fromCharCode(0xFFF0));
  var str2 = Array(4*(2*N + 1) + 1).join(String.fromCharCode(0xFFF0));
  gc();
  str1 = str2 = null;
  gc();

  var firstCall = true;
  a.sort(function (a, b) {
	   if (firstCall) {
	     firstCall = false;
	     gc();
	   }
	   return a - b;
	 });

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   igor@icesoft.no, pschwartau@netscape.com
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

/*
 * Date: 24 September 2001
 *
 * SUMMARY: Truncating arrays that have decimal property names.
 * From correspondence with Igor Bukanov <igor@icesoft.no>:
 */
//-----------------------------------------------------------------------------
var gTestfile = 'array-001.js';
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Truncating arrays that have decimal property names';
var BIG_INDEX = 4294967290;
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


var arr = Array(BIG_INDEX);
arr[BIG_INDEX - 1] = 'a';
arr[BIG_INDEX - 10000] = 'b';
arr[BIG_INDEX - 0.5] = 'c';  // not an array index - but a valid property name
// Truncate the array -
arr.length = BIG_INDEX - 5000;


// Enumerate its properties with for..in
var s = '';
for (var i in arr)
{
  s += arr[i];
}


/*
 * We expect s == 'cb' or 'bc' (EcmaScript does not fix the order).
 * Note 'c' is included: for..in includes ALL enumerable properties,
 * not just array-index properties. The bug was: Rhino gave s == ''.
 */
status = inSection(1);
actual = sortThis(s);
expect = 'bc';
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function sortThis(str)
{
  var chars = str.split('');
  chars = chars.sort();
  return chars.join('');
}


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}

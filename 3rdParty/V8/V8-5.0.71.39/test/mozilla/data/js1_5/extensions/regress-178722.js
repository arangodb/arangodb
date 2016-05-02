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
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   pschwartau@netscape.com
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
 *
 * Date:    06 November 2002
 * SUMMARY: arr.sort() should not output |undefined| when |arr| is empty
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=178722
 *
 * ECMA-262 Ed.3: 15.4.4.11 Array.prototype.sort (comparefn)
 *
 * 1. Call the [[Get]] method of this object with argument "length".
 * 2. Call ToUint32(Result(1)).
 * 3. Perform an implementation-dependent sequence of calls to the [[Get]],
 *    [[Put]], and [[Delete]] methods of this object, etc. etc.
 * 4. Return this object.
 *
 *
 * Note that sort() is done in-place on |arr|. In other words, sort() is a
 * "destructive" method rather than a "functional" method. The return value
 * of |arr.sort()| and |arr| are the same object.
 *
 * If |arr| is an empty array, the return value of |arr.sort()| should be
 * an empty array, not the value |undefined| as was occurring in bug 178722.
 *
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-178722.js';
var UBound = 0;
var BUGNUMBER = 178722;
var summary = 'arr.sort() should not output |undefined| when |arr| is empty';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var arr;


// create empty array or pseudo-array objects in various ways
function f () {return arguments};
var arr5 = f();
arr5.__proto__ = Array.prototype;


status = inSection(5);
arr = arr5.sort();
actual = arr instanceof Array && arr.length === 0 && arr === arr5;
expect = true;
addThis();


// now do the same thing, with non-default sorting:
function g() {return 1;}

status = inSection('5a');
arr = arr5.sort(g);
actual = arr instanceof Array && arr.length === 0 && arr === arr5;
expect = true;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}

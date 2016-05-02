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
 *   brendan@mozilla.org, pschwartau@netscape.com
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
 * Date: 07 May 2001
 *
 * SUMMARY:  Testing the arguments object
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=72884
 */
//-----------------------------------------------------------------------------
var gTestfile = 'arguments-001.js';
var UBound = 0;
var BUGNUMBER = 72884;
var summary = 'Testing the arguments object';
var status = '';
var statusitems = [ ];
var actual = '';
var actualvalues = [ ];
var expect= '';
var expectedvalues = [ ];
var a = '';


status = inSection(1);
function f()
{
  delete arguments.length;
  return arguments;
}

a = f();
actual = a instanceof Object;
expect = true;
addThis();

actual = a instanceof Array;
expect = false;
addThis();

actual = a.length;
expect = undefined;
addThis();



status = inSection(2);
a = f(1,2,3);
actual = a instanceof Object;
expect = true;
addThis();

actual = a instanceof Array;
expect = false;
addThis();

actual = a.length;
expect = undefined;
addThis();

actual = a[0];
expect = 1;
addThis();

actual = a[1];
expect = 2;
addThis();

actual = a[2];
expect = 3;
addThis();



status = inSection(3);
/*
 * Brendan:
 *
 * Note that only callee and length can be overridden, so deleting an indexed
 * property and asking for it again causes it to be recreated by args_resolve:
 *
 * function g(){delete arguments[0]; return arguments[0]}
 * g(42)     // should this print 42?
 *
 * I'm not positive this violates ECMA, which allows in chapter 16 for extensions
 * including properties (does it allow for magically reappearing properties?).  The
 * delete operator successfully deletes arguments[0] and results in true, but that
 * is not distinguishable from the case where arguments[0] was delegated to
 * Arguments.prototype[0], which was how the bad old code worked.
 *
 * I'll ponder this last detail...
 *
 * UPDATE: Per ECMA-262, delete on an arguments[i] should succeed
 * and remove that property from the arguments object, leaving any get
 * of it after the delete to evaluate to undefined.
 */
function g()
{
  delete arguments[0];
  return arguments[0];
}
actual = g(42);
expect = undefined;  // not 42...
addThis();



//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


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

  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}

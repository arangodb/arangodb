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
 *   deneen@alum.bucknell.edu, shaver@mozilla.org
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
 * Date: 08 August 2001
 *
 * SUMMARY: When we invoke a function, the arguments object should take
 *          a back seat to any local identifier named "arguments".
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=94506
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-94506.js';
var UBound = 0;
var BUGNUMBER = 94506;
var summary = 'Testing functions employing identifiers named "arguments"';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var TYPE_OBJECT = typeof new Object();
var arguments = 5555;


// use a parameter named "arguments"
function F1(arguments)
{
  return arguments;
}


// use a local variable named "arguments"
function F2()
{
  var arguments = 55;
  return arguments;
}


// same thing in a different order. CHANGES THE RESULT!
function F3()
{
  return arguments;
  var arguments = 555;
}


// use the global variable above named "arguments"
function F4()
{
  return arguments;
}



/*
 * In Sections 1 and 2, expect the local identifier, not the arguments object.
 * In Sections 3 and 4, expect the arguments object, not the the identifier.
 */

status = 'Section 1 of test';
actual = F1(5);
expect = 5;
addThis();


status = 'Section 2 of test';
actual = F2();
expect = 55;
addThis();


status = 'Section 3 of test';
actual = typeof F3();
expect = TYPE_OBJECT;
addThis();


status = 'Section 4 of test';
actual = typeof F4();
expect = TYPE_OBJECT;
addThis();


// Let's try calling F1 without providing a parameter -
status = 'Section 5 of test';
actual = F1();
expect = undefined;
addThis();


// Let's try calling F1 with too many parameters -
status = 'Section 6 of test';
actual = F1(3,33,333);
expect = 3;
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}

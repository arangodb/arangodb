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
 * Date: 2001-08-27
 *
 * SUMMARY:  Testing binding of function names
 *
 * Brendan:
 *
 * "... the question is, does Rhino bind 'sum' in the global object
 * for the following test? If it does, it's buggy.
 *
 *   var f = function sum(){};
 *   print(sum);  // should fail with 'sum is not defined' "
 *
 */

//-----------------------------------------------------------------------------
var gTestfile = 'binding-001.js';
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing binding of function names';
var ERR_REF_YES = 'ReferenceError';
var ERR_REF_NO = 'did NOT generate a ReferenceError';
var statusitems = [];
var actualvalues = [];
var expectedvalues = [];
var status = summary;
var actual = ERR_REF_NO;
var expect= ERR_REF_YES;


try
{
  var f = function sum(){};
  print(sum);
}
catch (e)
{
  status = 'Section 1 of test';
  actual = e instanceof ReferenceError;
  expect = true;
  addThis();


  /*
   * This test is more literal, and one day may not be valid.
   * Searching for literal string "ReferenceError" in e.toString()
   */
  status = 'Section 2 of test';
  var match = e.toString().search(/ReferenceError/);
  actual = (match > -1);
  expect = true;
  addThis();
}



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = isReferenceError(actual);
  expectedvalues[UBound] = isReferenceError(expect);
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


// converts a Boolean result into a textual result -
function isReferenceError(bResult)
{
  return bResult? ERR_REF_YES : ERR_REF_NO;
}

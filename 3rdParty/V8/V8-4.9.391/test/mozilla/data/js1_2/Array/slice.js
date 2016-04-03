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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

gTestfile = 'slice.js';

/**
   Filename:     slice.js
   Description:  'This tests out some of the functionality on methods on the Array objects'

   Author:       Nick Lerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'String:slice';

writeHeaderToLog('Executing script: slice.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var a = ['a','test string',456,9.34,new String("string object"),[],['h','i','j','k']];
var b = [1,2,3,4,5,6,7,8,9,0];

exhaustiveSliceTest("exhaustive slice test 1", a);
exhaustiveSliceTest("exhaustive slice test 2", b);

test();

function mySlice(a, from, to)
{
  var from2       = from;
  var to2         = to;
  var returnArray = [];
  var i;

  if (from2 < 0) from2 = a.length + from;
  if (to2 < 0)   to2   = a.length + to;

  if ((to2 > from2)&&(to2 > 0)&&(from2 < a.length))
  {
    if (from2 < 0)        from2 = 0;
    if (to2 > a.length) to2 = a.length;

    for (i = from2; i < to2; ++i) returnArray.push(a[i]);
  }
  return returnArray;
}

// This function tests the slice command on an Array
// passed in. The arguments passed into slice range in
// value from -5 to the length of the array + 4. Every
// combination of the two arguments is tested. The expected
// result of the slice(...) method is calculated and
// compared to the actual result from the slice(...) method.
// If the Arrays are not similar false is returned.
function exhaustiveSliceTest(testname, a)
{
  var x = 0;
  var y = 0;
  var errorMessage;
  var reason = "";
  var passed = true;

  for (x = -(2 + a.length); x <= (2 + a.length); x++)
    for (y = (2 + a.length); y >= -(2 + a.length); y--)
    {
      var b  = a.slice(x,y);
      var c = mySlice(a,x,y);

      if (String(b) != String(c))
      {
	errorMessage =
	  "ERROR: 'TEST FAILED' ERROR: 'TEST FAILED' ERROR: 'TEST FAILED'\n" +
	  "            test: " + "a.slice(" + x + "," + y + ")\n" +
	  "               a: " + String(a) + "\n" +
	  "   actual result: " + String(b) + "\n" +
	  " expected result: " + String(c) + "\n";
	writeHeaderToLog(errorMessage);
	reason = reason + errorMessage;
	passed = false;
      }
    }
  var testCase = new TestCase(SECTION, testname, true, passed);
  if (passed == false)
    testCase.reason = reason;
  return testCase;
}

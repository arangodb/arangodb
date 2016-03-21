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

gTestfile = 'splice2.js';

/**
   Filename:     splice2.js
   Description:  'Tests Array.splice(x,y) w/4 var args'

   Author:       Nick Lerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
var TITLE = 'String:splice 2';
var BUGNUMBER="123795";

startTest();
writeHeaderToLog('Executing script: splice2.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var a = ['a','test string',456,9.34,new String("string object"),[],['h','i','j','k']];
var b = [1,2,3,4,5,6,7,8,9,0];

exhaustiveSpliceTestWithArgs("exhaustive splice w/2 optional args 1",a);
exhaustiveSpliceTestWithArgs("exhaustive splice w/2 optional args 2",b);

test();


function mySplice(testArray, splicedArray, first, len, elements)
{
  var removedArray  = [];
  var adjustedFirst = first;
  var adjustedLen   = len;

  if (adjustedFirst < 0) adjustedFirst = testArray.length + first;
  if (adjustedFirst < 0) adjustedFirst = 0;

  if (adjustedLen < 0) adjustedLen = 0;

  for (i = 0; (i < adjustedFirst)&&(i < testArray.length); ++i)
    splicedArray.push(testArray[i]);

  if (adjustedFirst < testArray.length)
    for (i = adjustedFirst; (i < adjustedFirst + adjustedLen) && (i < testArray.length); ++i)
      removedArray.push(testArray[i]);

  for (i = 0; i < elements.length; i++) splicedArray.push(elements[i]);

  for (i = adjustedFirst + adjustedLen; i < testArray.length; i++)
    splicedArray.push(testArray[i]);

  return removedArray;
}

function exhaustiveSpliceTestWithArgs(testname, testArray)
{
  var passed = true;
  var errorMessage;
  var reason = "";
  for (var first = -(testArray.length+2); first <= 2 + testArray.length; first++)
  {
    var actualSpliced   = [];
    var expectedSpliced = [];
    var actualRemoved   = [];
    var expectedRemoved = [];

    for (var len = 0; len < testArray.length + 2; len++)
    {
      actualSpliced   = [];
      expectedSpliced = [];

      for (var i = 0; i < testArray.length; ++i)
	actualSpliced.push(testArray[i]);

      actualRemoved   = actualSpliced.splice(first,len,-97,new String("test arg"),[],9.8);
      expectedRemoved = mySplice(testArray,expectedSpliced,first,len,[-97,new String("test arg"),[],9.8]);

      var adjustedFirst = first;
      if (adjustedFirst < 0) adjustedFirst = testArray.length + first;
      if (adjustedFirst < 0) adjustedFirst = 0;


      if (  (String(actualSpliced) != String(expectedSpliced))
	    ||(String(actualRemoved) != String(expectedRemoved)))
      {
	if (  (String(actualSpliced) == String(expectedSpliced))
	      &&(String(actualRemoved) != String(expectedRemoved)) )
	{

	  if ( (expectedRemoved.length == 1)
	       &&(String(actualRemoved) == String(expectedRemoved[0]))) continue;
	  if ( expectedRemoved.length == 0  && actualRemoved == void 0 ) continue;
	}

	errorMessage =
	  "ERROR: 'TEST FAILED' ERROR: 'TEST FAILED' ERROR: 'TEST FAILED'\n" +
	  "             test: " + "a.splice(" + first + "," + len + ",-97,new String('test arg'),[],9.8)\n" +
	  "                a: " + String(testArray) + "\n" +
	  "   actual spliced: " + String(actualSpliced) + "\n" +
	  " expected spliced: " + String(expectedSpliced) + "\n" +
	  "   actual removed: " + String(actualRemoved) + "\n" +
	  " expected removed: " + String(expectedRemoved);
	reason = reason + errorMessage;
	writeHeaderToLog(errorMessage);
	passed = false;
      }
    }
  }
  var testcase = new TestCase(SECTION, testname, true, passed);
  if (!passed) testcase.reason = reason;
  return testcase;
}

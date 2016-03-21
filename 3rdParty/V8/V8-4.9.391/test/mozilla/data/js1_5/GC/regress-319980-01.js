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
 * Contributor(s): diablohn
 *                 WADA
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

var gTestfile = 'regress-319980-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 319980;
var summary = 'GC not called during non-fatal out of memory';
var actual = '';
var expect = 'Normal Exit';

printBugNumber(BUGNUMBER);
printStatus (summary);
print ('This test should never fail explicitly. ' +
       'You must view the memory usage during the test. ' +
       'This test fails if memory usage for each subtest grows');

var timeOut  = 45 * 1000;
var interval = 0.01  * 1000;
var testFuncWatcherId;
var testFuncTimerId;
var maxTests = 5;
var currTest = 0;

if (typeof setTimeout == 'undefined')
{
  setTimeout = function() {};
  clearTimeout = function() {};
  actual = 'Normal Exit';
  reportCompare(expect, actual, summary);
}
else
{
  // delay start until after js-test-driver-end runs.
  // delay test driver end
  gDelayTestDriverEnd = true;

  setTimeout(testFuncWatcher, 1000);
}

function testFuncWatcher()
{
  a = null;

  gc();

  clearTimeout(testFuncTimerId);
  testFuncWatcherId = testFuncTimerId = null;
  if (currTest >= maxTests)
  {
    actual = 'Normal Exit';
    reportCompare(expect, actual, summary);
    printStatus('Test Completed');
    gDelayTestDriverEnd = false;
    jsTestDriverEnd();
    return;
  }
  ++currTest;
 
  print('Executing test ' + currTest + '\n');

  testFuncWatcherId = setTimeout("testFuncWatcher()", timeOut);
  testFuncTimerId = setTimeout(testFunc, interval);
}


var a;
function testFunc()
{

  var i;

  switch(currTest)
  {
  case 1:
    a = new Array(100000);
    for (i = 0; i < 100000; i++ )
    {
      a[i] = i;
    }
    break;

  case 2:
    a = new Array(100000);
    for (i = 0; i < 100000; i++)
    {
      a[i] = new Number();
      a[i] = i;
    }
    break;

  case 3:
    a = new String() ;
    a = new Array(100000);
    for ( i = 0; i < 100000; i++ )
    {
      a[i] = i;
    }

    break;

  case 4:
    a = new Array();
    a[0] = new Array(100000);
    for (i = 0; i < 100000; i++ )
    {
      a[0][i] = i;
    }
    break;

  case 5:
    a = new Array();
    for (i = 0; i < 100000; i++ )
    {
      a[i] = i;
    }
    break;
  }

  if (testFuncTimerId)
  {
    testFuncTimerId = setTimeout(testFunc, interval);
  }
}



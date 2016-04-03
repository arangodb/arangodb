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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   chwu@nortelnetworks.com, timeless@mac.com,
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
 *
 * Date: 10 October 2001
 * SUMMARY: Regression test for Bugzilla bug 104077
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=104077
 * "JS crash: with/finally/return"
 *
 * Also http://bugzilla.mozilla.org/show_bug.cgi?id=120571
 * "JS crash: try/catch/continue."
 *
 * SpiderMonkey crashed on this code - it shouldn't.
 *
 * NOTE: the finally-blocks below should execute even if their try-blocks
 * have return or throw statements in them:
 *
 * ------- Additional Comment #76 From Mike Shaver 2001-12-07 01:21 -------
 * finally trumps return, and all other control-flow constructs that cause
 * program execution to jump out of the try block: throw, break, etc.  Once you
 * enter a try block, you will execute the finally block after leaving the try,
 * regardless of what happens to make you leave the try.
 *
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-104077.js';
var UBound = 0;
var BUGNUMBER = 104077;
var summary = "Just testing that we don't crash on with/finally/return -";
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


function addValues_3(obj)
{
  var sum = 0;
 
  with (obj)
  {
    try
    {
      sum = arg1 + arg2;
      with (arg3)
      {
        while (sum < 10)
        {
          try
          {
            if (sum > 5)
              return sum;
            sum += 1;
          }
          catch (e)
          {
            sum += 1;
            print(e);
          }
        }
      }
    }
    finally
    {
      try
      {
        sum +=1;
        print("In finally block of addValues_3() function: sum = " + sum);
      }
      catch (e if e == 42)
      {
        sum +=1;
        print('In finally catch block of addValues_3() function: sum = ' + sum + ', e = ' + e);
      }
      finally
      {
        sum +=1;
        print("In finally finally block of addValues_3() function: sum = " + sum);
        return sum;
      }
    }
  }
}

status = inSection(9);
obj = new Object();
obj.arg1 = 1;
obj.arg2 = 2;
obj.arg3 = new Object();
obj.arg3.a = 10;
obj.arg3.b = 20;
actual = addValues_3(obj);
expect = 8;
captureThis();




function addValues_4(obj)
{
  var sum = 0;

  with (obj)
  {
    try
    {
      sum = arg1 + arg2;
      with (arg3)
      {
        while (sum < 10)
        {
          try
          {
            if (sum > 5)
              return sum;
            sum += 1;
          }
          catch (e)
          {
            sum += 1;
            print(e);
          }
        }
      }
    }
    finally
    {
      try
      {
        sum += 1;
        print("In finally block of addValues_4() function: sum = " + sum);
      }
      catch (e if e == 42)
      {
        sum += 1;
        print("In 1st finally catch block of addValues_4() function: sum = " + sum + ", e = " + e);
      }
      catch (e if e == 43)
      {
        sum += 1;
        print("In 2nd finally catch block of addValues_4() function: sum = " + sum + ", e = " + e);
      }
      finally
      {
        sum += 1;
        print("In finally finally block of addValues_4() function: sum = " + sum);
        return sum;
      }
    }
  }
}

status = inSection(10);
obj = new Object();
obj.arg1 = 1;
obj.arg2 = 2;
obj.arg3 = new Object();
obj.arg3.a = 10;
obj.arg3.b = 20;
actual = addValues_4(obj);
expect = 8;
captureThis();




//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function captureThis()
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

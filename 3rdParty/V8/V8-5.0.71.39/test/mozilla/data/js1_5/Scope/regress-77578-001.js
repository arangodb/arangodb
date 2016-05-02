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
 *   jrgm@netscape.com, pschwartau@netscape.com
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
 * Date: 2001-07-11
 *
 * SUMMARY: Testing eval scope inside a function.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=77578
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-77578-001.js';
var UBound = 0;
var BUGNUMBER = 77578;
var summary = 'Testing eval scope inside a function';
var cnEquals = '=';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


// various versions of JavaScript -
var JS_VER = [100, 110, 120, 130, 140, 150];

// Note contrast with local variables i,j,k defined below -
var i = 999;
var j = 999;
var k = 999;


//--------------------------------------------------
test();
//--------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  // Run tests A,B,C on each version of JS and store results
  for (var n=0; n!=JS_VER.length; n++)
  {
    testA(JS_VER[n]);
  }
  for (var n=0; n!=JS_VER.length; n++)
  {
    testB(JS_VER[n]);
  }
  for (var n=0; n!=JS_VER.length; n++)
  {
    testC(JS_VER[n]);
  }


  // Compare actual values to expected values -
  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}


function testA(ver)
{
  // Set the version of JS to test -
  if (typeof version == 'function')
  {
    version(ver);
  }

  // eval the test, so it compiles AFTER version() has executed -
  var sTestScript = "";

  // Define a local variable i
  sTestScript += "status = 'Section A of test; JS ' + ver/100;";
  sTestScript += "var i=1;";
  sTestScript += "actual = eval('i');";
  sTestScript += "expect = 1;";
  sTestScript += "captureThis('i');";

  eval(sTestScript);
}


function testB(ver)
{
  // Set the version of JS to test -
  if (typeof version == 'function')
  {
    version(ver);
  }

  // eval the test, so it compiles AFTER version() has executed -
  var sTestScript = "";

  // Define a local for-loop iterator j
  sTestScript += "status = 'Section B of test; JS ' + ver/100;";
  sTestScript += "for(var j=1; j<2; j++)";
  sTestScript += "{";
  sTestScript += "  actual = eval('j');";
  sTestScript += "};";
  sTestScript += "expect = 1;";
  sTestScript += "captureThis('j');";

  eval(sTestScript);
}


function testC(ver)
{
  // Set the version of JS to test -
  if (typeof version == 'function')
  {
    version(ver);
  }

  // eval the test, so it compiles AFTER version() has executed -
  var sTestScript = "";

  // Define a local variable k in a try-catch block -
  sTestScript += "status = 'Section C of test; JS ' + ver/100;";
  sTestScript += "try";
  sTestScript += "{";
  sTestScript += "  var k=1;";
  sTestScript += "  actual = eval('k');";
  sTestScript += "}";
  sTestScript += "catch(e)";
  sTestScript += "{";
  sTestScript += "};";
  sTestScript += "expect = 1;";
  sTestScript += "captureThis('k');";

  eval(sTestScript);
}


function captureThis(varName)
{
  statusitems[UBound] = status;
  actualvalues[UBound] = varName + cnEquals + actual;
  expectedvalues[UBound] = varName + cnEquals + expect;
  UBound++;
}

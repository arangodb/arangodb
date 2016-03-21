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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   zack-weg@gmx.de, PhilSchwartau@aol.com
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
 * Date:    08 November 2003
 * SUMMARY: |expr()| should cause a TypeError if |typeof expr| != 'function'
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=224956
 *
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-224956.js';
var UBound = 0;
var BUGNUMBER = 224956;
var summary = "|expr()| should cause TypeError if |typeof expr| != 'function'";
var TEST_PASSED = 'TypeError';
var TEST_FAILED = 'Generated an error, but NOT a TypeError! ';
var TEST_FAILED_BADLY = 'Did not generate ANY error!!!';
var CHECK_PASSED = 'Should not generate an error';
var CHECK_FAILED = 'Generated an error!';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var x;


/*
 * All the following contain invalid uses of the call operator ()
 * and should generate TypeErrors. That's what we're testing for.
 *
 * To save writing try...catch over and over, we hide all the
 * errors inside eval strings, and let testThis() catch them.
 */
status = inSection(1);
x = 'abc';
testThis('x()');

status = inSection(2);
testThis('"abc"()');

status = inSection(3);
x = new Date();
testThis('x()');

status = inSection(4);
testThis('Date(12345)()');

status = inSection(5);
x = Number(1);
testThis('x()');

status = inSection(6);
testThis('1()');

status = inSection(7);
x = void(0);
testThis('x()');

status = inSection(8);
testThis('void(0)()');

status = inSection(9);
x = Math;
testThis('x()');

status = inSection(10);
testThis('Math()');

status = inSection(11);
x = Array(5);
testThis('x()');

status = inSection(12);
testThis('[1,2,3,4,5]()');

status = inSection(13);
x = [1,2,3].splice(1,2);
testThis('x()');


/*
 * Same as above, but with non-empty call parentheses
 */
status = inSection(14);
x = 'abc';
testThis('x(1)');

status = inSection(15);
testThis('"abc"(1)');

status = inSection(16);
x = new Date();
testThis('x(1)');

status = inSection(17);
testThis('Date(12345)(1)');

status = inSection(18);
x = Number(1);
testThis('x(1)');

status = inSection(19);
testThis('1(1)');

status = inSection(20);
x = void(0);
testThis('x(1)');

status = inSection(21);
testThis('void(0)(1)');

status = inSection(22);
x = Math;
testThis('x(1)');

status = inSection(23);
testThis('Math(1)');

status = inSection(24);
x = Array(5);
testThis('x(1)');

status = inSection(25);
testThis('[1,2,3,4,5](1)');

status = inSection(26);
x = [1,2,3].splice(1,2);
testThis('x(1)');


/*
 * Expression from website in original bug report above -
 */
status = inSection(27);
var A = 1, C=2, Y=3, T=4, I=5;
testThis('(((C/A-0.3)/0.2)+((Y/A-3)/4)+((T/A)/0.05)+((0.095-I/A)/0.4))(100/6)');



/*
 * Functions and RegExps both have |typeof| == 'function'.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=61911.
 *
 * Therefore these expressions should not cause any errors.
 * Note we use checkThis() instead of testThis()
 *
 */
status = inSection(28);
x = function (y) {return y+1;};
checkThis('x("abc")');

status = inSection(29);
checkThis('(function (y) {return y+1;})("abc")');

status = inSection(30);
function f(y) { function g() {return y;}; return g();};
checkThis('f("abc")');

status = inSection(31);
x = /a()/;
checkThis('x("abc")');

status = inSection(32);
x = /a()/gi;
checkThis('x("abc")');

status = inSection(33);
x = RegExp('a()');
checkThis('x("abc")');

status = inSection(34);
x = new RegExp('a()', 'gi');
checkThis('x("")');




//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------




/*
 * |expr()| should generate a TypeError if |expr| is not a function
 */
function testThis(sInvalidSyntax)
{
  expect = TEST_PASSED;
  actual = TEST_FAILED_BADLY;

  try
  {
    eval(sInvalidSyntax);
  }
  catch(e)
  {
    if (e instanceof TypeError)
      actual = TEST_PASSED;
    else
      actual = TEST_FAILED + e;
  }

  statusitems[UBound] = status;
  expectedvalues[UBound] = expect;
  actualvalues[UBound] = actual;
  UBound++;
}


/*
 * Valid syntax shouldn't generate any errors
 */
function checkThis(sValidSyntax)
{
  expect = CHECK_PASSED;
  actual = CHECK_PASSED;

  try
  {
    eval(sValidSyntax);
  }
  catch(e)
  {
    actual = CHECK_FAILED;
  }

  statusitems[UBound] = status;
  expectedvalues[UBound] = expect;
  actualvalues[UBound] = actual;
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

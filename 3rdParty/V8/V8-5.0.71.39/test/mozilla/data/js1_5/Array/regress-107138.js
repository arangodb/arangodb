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
 *   morse@netscape.com, pschwartau@netscape.com
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
 * Date: 29 October 2001
 *
 * SUMMARY: Regression test for bug 107138
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=107138
 *
 * The bug: arr['1'] == undefined instead of arr['1'] == 'one'.
 * The bug was intermittent and did not always occur...
 *
 * The cnSTRESS constant defines how many times to repeat this test.
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-107138.js';
var UBound = 0;
var cnSTRESS = 10;
var cnDASH = '-';
var BUGNUMBER = 107138;
var summary = 'Regression test for bug 107138';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


var arr = ['zero', 'one', 'two', 'three', 'four', 'five',
           'six', 'seven', 'eight', 'nine', 'ten'];


// This bug was intermittent. Stress-test it.
for (var j=0; j<cnSTRESS; j++)
{
  status = inSection(j + cnDASH + 1);
  actual = arr[0];
  expect = 'zero';
  addThis();

  status = inSection(j + cnDASH + 2);
  actual = arr['0'];
  expect = 'zero';
  addThis();

  status = inSection(j + cnDASH + 3);
  actual = arr[1];
  expect = 'one';
  addThis();

  status = inSection(j + cnDASH + 4);
  actual = arr['1'];
  expect = 'one';
  addThis();

  status = inSection(j + cnDASH + 5);
  actual = arr[2];
  expect = 'two';
  addThis();

  status = inSection(j + cnDASH + 6);
  actual = arr['2'];
  expect = 'two';
  addThis();

  status = inSection(j + cnDASH + 7);
  actual = arr[3];
  expect = 'three';
  addThis();

  status = inSection(j + cnDASH + 8);
  actual = arr['3'];
  expect = 'three';
  addThis();

  status = inSection(j + cnDASH + 9);
  actual = arr[4];
  expect = 'four';
  addThis();

  status = inSection(j + cnDASH + 10);
  actual = arr['4'];
  expect = 'four';
  addThis();

  status = inSection(j + cnDASH + 11);
  actual = arr[5];
  expect = 'five';
  addThis();

  status = inSection(j + cnDASH + 12);
  actual = arr['5'];
  expect = 'five';
  addThis();

  status = inSection(j + cnDASH + 13);
  actual = arr[6];
  expect = 'six';
  addThis();

  status = inSection(j + cnDASH + 14);
  actual = arr['6'];
  expect = 'six';
  addThis();

  status = inSection(j + cnDASH + 15);
  actual = arr[7];
  expect = 'seven';
  addThis();

  status = inSection(j + cnDASH + 16);
  actual = arr['7'];
  expect = 'seven';
  addThis();

  status = inSection(j + cnDASH + 17);
  actual = arr[8];
  expect = 'eight';
  addThis();

  status = inSection(j + cnDASH + 18);
  actual = arr['8'];
  expect = 'eight';
  addThis();

  status = inSection(j + cnDASH + 19);
  actual = arr[9];
  expect = 'nine';
  addThis();

  status = inSection(j + cnDASH + 20);
  actual = arr['9'];
  expect = 'nine';
  addThis();

  status = inSection(j + cnDASH + 21);
  actual = arr[10];
  expect = 'ten';
  addThis();

  status = inSection(j + cnDASH + 22);
  actual = arr['10'];
  expect = 'ten';
  addThis();
}


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

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}

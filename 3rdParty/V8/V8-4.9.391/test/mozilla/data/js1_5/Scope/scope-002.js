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
 *   pschwartau@netscape.com, crock@veilnetworks.com
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
 * Date: 2001-07-02
 *
 * SUMMARY:  Testing visibility of outer function from inner function.
 *
 */
//-----------------------------------------------------------------------------
var gTestfile = 'scope-002.js';
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing visibility of outer function from inner function';
var cnCousin = 'Fred';
var cnColor = 'red';
var cnMake = 'Toyota';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


// TEST 1
function Outer()
{

  function inner()
  {
    Outer.cousin = cnCousin;
    return Outer.cousin;
  }

  status = 'Section 1 of test';
  actual = inner();
  expect = cnCousin;
  addThis();
}


Outer();
status = 'Section 2 of test';
actual = Outer.cousin;
expect = cnCousin;
addThis();



// TEST 2
function Car(make)
{
  this.make = make;
  Car.prototype.paint = paint;

  function paint()
  {
    Car.color = cnColor;
    Car.prototype.color = Car.color;
  }
}


var myCar = new Car(cnMake);
status = 'Section 3 of test';
actual = myCar.make;
expect = cnMake;
addThis();


myCar.paint();
status = 'Section 4 of test';
actual = myCar.color;
expect = cnColor;
addThis();



//--------------------------------------------------
test();
//--------------------------------------------------



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

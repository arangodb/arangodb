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
 *   pschwartau@netscape.com
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

var gTestfile = 'scope-001.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = '53268';
var status = 'Testing scope after changing obj.__proto__';
var expect= '';
var actual = '';
var obj = {};
const five = 5;


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  enterFunc ("test");
  printBugNumber(BUGNUMBER);
  printStatus (status);


  status= 'Step 1:  setting obj.__proto__ = global object';
  obj.__proto__ = this;

  actual = obj.five;
  expect=5;
  reportCompare (expect, actual, status);

  obj.five=1;
  actual = obj.five;
  expect=5;
  reportCompare (expect, actual, status);



  status= 'Step 2:  setting obj.__proto__ = null';
  obj.__proto__ = null;

  actual = obj.five;
  expect=undefined;
  reportCompare (expect, actual, status);

  obj.five=2;
  actual = obj.five;
  expect=2;
  reportCompare (expect, actual, status);


 
  status= 'Step 3:  setting obj.__proto__  to global object again';
  obj.__proto__ = this;

  actual = obj.five;
  expect=2;  //<--- (FROM STEP 2 ABOVE)
  reportCompare (expect, actual, status);

  obj.five=3;
  actual = obj.five;
  expect=3;
  reportCompare (expect, actual, status);



  status= 'Step 4:  setting obj.__proto__   to  null again';
  obj.__proto__ = null;

  actual = obj.five;
  expect=3;  //<--- (FROM STEP 3 ABOVE)
  reportCompare (expect, actual, status);

  obj.five=4;
  actual = obj.five;
  expect=4;
  reportCompare (expect, actual, status);


  exitFunc ("test");
}

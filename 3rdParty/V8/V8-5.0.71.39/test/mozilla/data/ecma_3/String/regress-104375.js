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
 *   k.mike@gmx.net, pschwartau@netscape.com
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
 * Date: 12 October 2001
 *
 * SUMMARY: Regression test for string.replace bug 104375
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=104375
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-104375.js';
var UBound = 0;
var BUGNUMBER = 104375;
var summary = 'Testing string.replace() with backreferences';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * Use the regexp to replace 'uid=31' with 'uid=15'
 *
 * In the second parameter of string.replace() method,
 * "$1" refers to the first backreference: 'uid='
 */
var str = 'uid=31';
var re = /(uid=)(\d+)/;

// try the numeric literal 15
status = inSection(1);
actual  = str.replace (re, "$1" + 15);
expect = 'uid=15';
addThis();

// try the string literal '15'
status = inSection(2);
actual  = str.replace (re, "$1" + '15');
expect = 'uid=15';
addThis();

// try a letter before the '15'
status = inSection(3);
actual  = str.replace (re, "$1" + 'A15');
expect = 'uid=A15';
addThis();



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



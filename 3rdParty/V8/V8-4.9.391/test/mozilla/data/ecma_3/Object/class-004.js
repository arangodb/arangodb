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

/*
 * Date: 14 Mar 2001
 *
 * SUMMARY: Testing [[Class]] property of native error constructors.
 * See ECMA-262 Edition 3, Section 8.6.2 for the [[Class]] property.
 *
 * See ECMA-262 Edition 3, Section 15.11.6 for the native error types.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=56868
 *
 * Same as class-003.js - but testing the constructors here, not
 * object instances.  Therefore we expect the [[Class]] property to
 * equal 'Function' in each case.
 *
 * The getJSClass() function we use is in a utility file, e.g. "shell.js"
 */
//-----------------------------------------------------------------------------
var gTestfile = 'class-004.js';
var i = 0;
var UBound = 0;
var BUGNUMBER = 56868;
var summary = 'Testing the internal [[Class]] property of native error constructors';
var statprefix = 'Current constructor is: ';
var status = ''; var statusList = [ ];
var actual = ''; var actualvalue = [ ];
var expect= ''; var expectedvalue = [ ];

/*
 * We set the expect variable each time only for readability.
 * We expect 'Function' every time; see discussion above -
 */
status = 'Error';
actual = getJSClass(Error);
expect = 'Function';
addThis();

status = 'EvalError';
actual = getJSClass(EvalError);
expect = 'Function';
addThis();

status = 'RangeError';
actual = getJSClass(RangeError);
expect = 'Function';
addThis();

status = 'ReferenceError';
actual = getJSClass(ReferenceError);
expect = 'Function';
addThis();

status = 'SyntaxError';
actual = getJSClass(SyntaxError);
expect = 'Function';
addThis();

status = 'TypeError';
actual = getJSClass(TypeError);
expect = 'Function';
addThis();

status = 'URIError';
actual = getJSClass(URIError);
expect = 'Function';
addThis();



//---------------------------------------------------------------------------------
test();
//---------------------------------------------------------------------------------



function addThis()
{
  statusList[UBound] = status;
  actualvalue[UBound] = actual;
  expectedvalue[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalue[i], actualvalue[i], getStatus(i));
  }

  exitFunc ('test');
}


function getStatus(i)
{
  return statprefix + statusList[i];
}

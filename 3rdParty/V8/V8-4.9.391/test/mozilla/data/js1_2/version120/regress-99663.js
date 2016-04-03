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

gTestfile = 'regress-99663.js';

//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 99663;
var summary = 'Regression test for Bugzilla bug 99663';
/*
 * This testcase expects error messages containing
 * the phrase 'read-only' or something similar -
 */
var READONLY = /read\s*-?\s*only/;
var READONLY_TRUE = 'a "read-only" error';
var READONLY_FALSE = 'Error: ';
var FAILURE = 'NO ERROR WAS GENERATED!';
var status = '';
var actual = '';
var expect= '';
var statusitems = [];
var expectedvalues = [];
var actualvalues = [];


/*
 * These MUST be compiled in JS1.2 or less for the test to work - see above
 */
function f1()
{
  with (it)
  {
    for (rdonly in this);
  }
}


function f2()
{
  for (it.rdonly in this);
}


function f3(s)
{
  for (it[s] in this);
}



/*
 * Begin testing by capturing actual vs. expected values.
 * Initialize to FAILURE; this will get reset if all goes well -
 */
actual = FAILURE;
try
{
  f1();
}
catch(e)
{
  actual = readOnly(e.message);
}
expect= READONLY_TRUE;
status = 'Section 1 of test - got ' + actual;
addThis();


actual = FAILURE;
try
{
  f2();
}
catch(e)
{
  actual = readOnly(e.message);
}
expect= READONLY_TRUE;
status = 'Section 2 of test - got ' + actual;
addThis();


actual = FAILURE;
try
{
  f3('rdonly');
}
catch(e)
{
  actual = readOnly(e.message);
}
expect= READONLY_TRUE;
status = 'Section 3 of test - got ' + actual;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function readOnly(msg)
{
  if (msg.match(READONLY))
    return READONLY_TRUE;
  return READONLY_FALSE + msg;
}


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  print ('Bug Number ' + bug);
  print ('STATUS: ' + summary);

  for (var i=0; i<UBound; i++)
  {
    writeTestCaseResult(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}

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
 * ***** END LICENSE BLOCK ***** /

/*
 * Date: 27 September 2001
 *
 * SUMMARY: Performance: truncating even very large arrays should be fast!
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=101964
 *
 * Adjust this testcase if necessary. The FAST constant defines
 * an upper bound in milliseconds for any truncation to take.
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-101964.js';
var UBound = 0;
var BUGNUMBER = 101964;
var summary = 'Performance: truncating even very large arrays should be fast!';
var BIG = 10000000;
var LITTLE = 10;
var FAST = 50; // array truncation should be 50 ms or less to pass the test
var MSG_FAST = 'Truncation took less than ' + FAST + ' ms';
var MSG_SLOW = 'Truncation took ';
var MSG_MS = ' ms';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];



status = inSection(1);
var arr = Array(BIG);
var start = new Date();
arr.length = LITTLE;
actual = elapsedTime(start);
expect = FAST;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function elapsedTime(startTime)
{
  return new Date() - startTime;
}


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = isThisFast(actual);
  expectedvalues[UBound] = isThisFast(expect);
  UBound++;
}


function isThisFast(ms)
{
  if (ms <= FAST)
    return MSG_FAST;
  return MSG_SLOW + ms + MSG_MS;
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

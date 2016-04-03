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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Brendan Eich <brendan@mozilla.org>
 *                 Boris Zbarsky  <bzbarsky@mit.edu>
 *                 Steve Chapel <steven.chapel@sbcglobal.net>
 *                 Bob Clary <bob@bclary.com>
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

var gTestfile = 'regress-246964.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 246964;
// see also bug 248549, bug 253150, bug 259935
var summary = 'undetectable document.all';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof document == 'undefined')
{
  expect = actual = 'Test requires browser: skipped';
  reportCompare(expect, actual, summary);
}
else
{
  status = summary + ' ' + inSection(1) + ' if (document.all) ';
  expect = false;
  actual = false;
  if (document.all)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(2) + 'if (isIE) ';
  expect = false;
  actual = false;
  var isIE = document.all;
  if (isIE)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(3) + ' if (document.all != undefined) ';
  expect = false;
  actual = false;
  if (document.all != undefined)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);


  status = summary + ' ' + inSection(4) + ' if (document.all !== undefined) ';
  expect = false;
  actual = false;
  if (document.all !== undefined)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(5) + ' if (document.all != null) ' ;
  expect = false;
  actual = false;
  if (document.all != null)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(6) + ' if (document.all !== null) ' ;
  expect = true;
  actual = false;
  if (document.all !== null)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(7) + ' if (document.all == null) ';
  expect = true;
  actual = false;
  if (document.all == null)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(8) + ' if (document.all === null) ';
  expect = false;
  actual = false;
  if (document.all === null)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(9) + ' if (document.all == undefined) ';
  expect = true;
  actual = false;
  if (document.all == undefined)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(10) + ' if (document.all === undefined) ';
  expect = true;
  actual = false;
  if (document.all === undefined)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(11) +
    ' if (typeof document.all == "undefined") ';
  expect = true;
  actual = false;
  if (typeof document.all == 'undefined')
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(12) +
    ' if (typeof document.all != "undefined") ';
  expect = false;
  actual = false;
  if (typeof document.all != 'undefined')
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(13) + ' if ("all" in document) ';
  expect = (document.compatMode == 'CSS1Compat') ? false : true;
  actual = false;
  if ('all' in document)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(14) + ' if (f.ie) ';
  var f = new foo();

  expect = false;
  actual = false;
  if (f.ie)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

}

function foo()
{
  this.ie = document.all;
}

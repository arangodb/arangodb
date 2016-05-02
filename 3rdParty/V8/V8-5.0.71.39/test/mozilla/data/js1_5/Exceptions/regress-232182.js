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
 * Contributor(s): Michael Daumling <daumling@adobe.com>
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

var gTestfile = 'regress-232182.js';
//-----------------------------------------------------------------------------

var BUGNUMBER = 232182;
var summary = 'Display non-ascii characters in JS exceptions';
var actual = '';
var expect = 'no error';

printBugNumber(BUGNUMBER);
printStatus (summary);

/*
 * This test accesses an undefined Unicode symbol. If the code has not been
 * compiled with JS_C_STRINGS_ARE_UTF8, the thrown error truncates Unicode
 * characters to bytes. Accessing \u0440\u0441, therefore, results in a
 * message talking about an undefined variable 'AB' (\x41\x42).
 */
var utf8Enabled = false;
try
{
  \u0441\u0442;
}
catch (e)
{
  utf8Enabled = (e.message.charAt (0) == '\u0441');
}

// Run the tests only if UTF-8 is enabled

printStatus('UTF-8 is ' + (utf8Enabled?'':'not ') + 'enabled');

if (!utf8Enabled)
{
  reportCompare('Not run', 'Not run', 'utf8 is not enabled');
}
else
{
  status = summary + ': Throw Error with Unicode message';
  expect = 'test \u0440\u0441';
  try
  {
    throw Error (expect);
  }
  catch (e)
  {
    actual = e.message;
  }
  reportCompare(expect, actual, status);

  var inShell = (typeof stringsAreUTF8 == "function");
  if (!inShell)
  {
    inShell = (typeof stringsAreUtf8  == "function");
    if (inShell)
    {
      this.stringsAreUTF8 = stringsAreUtf8;
      this.testUTF8 = testUtf8;
    }
  }

  if (inShell && stringsAreUTF8())
  {
    status = summary + ': UTF-8 test: bad UTF-08 sequence';
    expect = 'Error';
    actual = 'No error!';
    try
    {
      testUTF8(1);
    }
    catch (e)
    {
      actual = 'Error';
    }
    reportCompare(expect, actual, status);

    status = summary + ': UTF-8 character too big to fit into Unicode surrogate pairs';
    expect = 'Error';
    actual = 'No error!';
    try
    {
      testUTF8(2);
    }
    catch (e)
    {
      actual = 'Error';
    }
    reportCompare(expect, actual, status);

    status = summary + ': bad Unicode surrogate character';
    expect = 'Error';
    actual = 'No error!';
    try
    {
      testUTF8(3);
    }
    catch (e)
    {
      actual = 'Error';
    }
    reportCompare(expect, actual, status);
  }

  if (inShell)
  {
    status = summary + ': conversion target buffer overrun';
    expect = 'Error';
    actual = 'No error!';
    try
    {
      testUTF8(4);
    }
    catch (e)
    {
      actual = 'Error';
    }
    reportCompare(expect, actual, status);
  }
}

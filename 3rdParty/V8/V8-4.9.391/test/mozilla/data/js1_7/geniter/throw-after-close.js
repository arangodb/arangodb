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
 * Jeff Walden.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

var gTestfile = 'throw-after-close.js';
//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "gen.close(); gen.throw(ex) throws ex forever";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function gen()
{
  var x = 5, y = 7;
  var z = x + y;
  yield z;
}

var failed = false;
var it = gen();

try
{
  it.close();

  // throw on closed generators just rethrows
  var doThrow = true;
  var thrown = "foobar";
  try
  {
    it.throw(thrown);
  }
  catch (e)
  {
    if (e === thrown)
      doThrow = false;
  }

  if (doThrow)
    throw "it.throw(\"" + thrown + "\") failed";

  // you can throw stuff at a closed generator forever
  doThrow = true;
  thrown = "sparky";
  try
  {
    it.throw(thrown);
  }
  catch (e)
  {
    if (e === thrown)
      doThrow = false;
  }

  if (doThrow)
    throw "it.throw(\"" + thrown + "\") failed";

  // don't execute a yield -- the uncaught exception
  // exhausted the generator
  var stopPassed = false;
  try
  {
    it.next();
  }
  catch (e)
  {
    if (e === StopIteration)
      stopPassed = true;
  }

  if (!stopPassed)
    throw "invalid or incorrect StopIteration";
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);

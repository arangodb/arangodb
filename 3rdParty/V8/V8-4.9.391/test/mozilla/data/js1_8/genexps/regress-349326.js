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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Igor Bukanov
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

var gTestfile = 'regress-349326.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 349326;
var summary = 'closing generators';
var actual = 'PASS';
var expect = 'PASS';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  let closed;

  function gen()
  {
    try {
      yield 1;
      yield 2;
    } finally {
      closed = true;
    }
  }

// Test that return closes the generator
  function test_return()
  {
    for (let i in gen()) {
      if (i != 1)
        throw "unexpected generator value";
      return 10;
    }
  }

  closed = false;
  test_return();
  if (closed !== true)
    throw "return does not close generator";

// test that break closes generator

  closed = false;
  for (let i in gen()) {
    if (i != 1)
      throw "unexpected generator value";
    break;
  }
  if (closed !== true)
    throw "break does not close generator";

label: {
    for (;;) {
      closed = false;
      for (let i in gen()) {
        if (i != 1)
          throw "unexpected generator value";
        break label;
      }
    }
  }

  if (closed !== true)
    throw "break does not close generator";

// test that an exception closes generator

  function function_that_throws()
  {
    throw function_that_throws;
  }

  try {
    closed = false;
    for (let i in gen()) {
      if (i != 1)
        throw "unexpected generator value";
      function_that_throws();
    }
  } catch (e) {
    if (e !== function_that_throws)
      throw e;
  }

  if (closed !== true)
    throw "exception does not close generator";

// Check consistency of finally execution in presence of generators

  let gen2_was_closed = false;
  let gen3_was_closed = false;
  let finally_was_executed = false;

  function gen2() {
    try {
      yield 2;
    } finally {
      if (gen2_was_closed || !finally_was_executed || !gen3_was_closed)
        throw "bad oder of gen2 finally execution"
          gen2_was_closed = true;
      throw gen2;
    }
  }

  function gen3() {
    try {
      yield 3;
    } finally {
      if (gen2_was_closed || finally_was_executed || gen3_was_closed)
        throw "bad oder of gen3 finally execution"
          gen3_was_closed = true;
    }
  }

label2: {
    try {
      for (let i in gen2()) {
        try {
          for (let j in gen3()) {
            break label2;
          }
        } finally {
          if (gen2_was_closed || finally_was_executed || !gen3_was_closed)
            throw "bad oder of try-finally execution";
          finally_was_executed = true;
        }
      }
      throw "gen2 finally should throw";
    } catch (e) {
      if (e != gen2) throw e;
    }
  }

  print("OK");

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

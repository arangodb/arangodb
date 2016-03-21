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
 * Netscape Communication Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

gTestfile = 'try-012.js';

/**
 *  File Name:          try-012.js
 *  ECMA Section:
 *  Description:        The try statement
 *
 *  This test has a try with no catch, and a finally.  This is like try-003,
 *  but throws from a finally block, not the try block.
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "try-012";
var VERSION = "ECMA_2";
var TITLE   = "The try statement";
var BUGNUMBER="336872";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

// Tests start here.

TrySomething( "x = \"hi\"", true );
TrySomething( "throw \"boo\"", true );
TrySomething( "throw 3", true );

test();

/**
 *  This function contains a try block with no catch block,
 *  but it does have a finally block.  Try to evaluate expressions
 *  that do and do not throw exceptions.
 *
 * The productioni TryStatement Block Finally is evaluated as follows:
 * 1. Evaluate Block
 * 2. Evaluate Finally
 * 3. If Result(2).type is normal return result 1 (in the test case, result 1 has
 *    the completion type throw)
 * 4. return result 2 (does not get hit in this case)
 *
 */

function TrySomething( expression, throwing ) {
  innerFinally = "FAIL: DID NOT HIT INNER FINALLY BLOCK";
  if (throwing) {
    outerCatch = "FAILED: NO EXCEPTION CAUGHT";
  } else {
    outerCatch = "PASS";
  }
  outerFinally = "FAIL: DID NOT HIT OUTER FINALLY BLOCK";


  // If the inner finally does not throw an exception, the result
  // of the try block should be returned.  (Type of inner return
  // value should be throw if finally executes correctly

  try {
    try {
      throw 0;
    } finally {
      innerFinally = "PASS";
      eval( expression );
    }
  } catch ( e  ) {
    if (throwing) {
      outerCatch = "PASS";
    } else {
      outerCatch = "FAIL: HIT OUTER CATCH BLOCK";
    }
  } finally {
    outerFinally = "PASS";
  }


  new TestCase(
    SECTION,
    "eval( " + expression +" ): evaluated inner finally block",
    "PASS",
    innerFinally );
  new TestCase(
    SECTION,
    "eval( " + expression +" ): evaluated outer catch block ",
    "PASS",
    outerCatch );
  new TestCase(
    SECTION,
    "eval( " + expression +" ):  evaluated outer finally block",
    "PASS",
    outerFinally );
}

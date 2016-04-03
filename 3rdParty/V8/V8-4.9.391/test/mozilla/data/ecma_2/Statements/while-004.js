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

gTestfile = 'while-004.js';

/**
 *  File Name:          while-004
 *  ECMA Section:
 *  Description:        while statement
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "while-004";
var VERSION = "ECMA_2";
var TITLE   = "while statement";
var BUGNUMBER="316725";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

DoWhile_1();
DoWhile_2();
DoWhile_3();
DoWhile_4();
DoWhile_5();

test();

/**
 *  Break out of a while by calling return.
 *
 *  Tests:  12.6.2 step 6.
 */
function dowhile() {
  result = "pass";

  while (true) {
    return result;
    result = "fail: hit code after return statement";
    break;
  }
}

function DoWhile_1() {
  description = "return statement in a while block";

  result = dowhile();

  new TestCase(
    SECTION,
    "DoWhile_1" + description,
    "pass",
    result );
}

/**
 *  While with a labeled continue statement.  Verify that statements
 *  after the continue statement are not evaluated.
 *
 *  Tests: 12.6.2 step 8.
 *
 */
function DoWhile_2() {
  var description = "while with a labeled continue statement";
  var result1 = "pass";
  var result2 = "fail: did not execute code after loop, but inside label";
  var i = 0;
  var j = 0;

theloop:
  while( i++ < 10  ) {
    j++;
    continue theloop;
    result1 = "failed:  hit code after continue statement";
  }
  result2 = "pass";

  new TestCase(
    SECTION,
    "DoWhile_2:  " +description + " - code inside the loop, before the continue should be executed ("+j+")",
    true,
    j == 10 );

  new TestCase(
    SECTION,
    "DoWhile_2:  " +description +" - code after labeled continue should not be executed",
    "pass",
    result1 );

  new TestCase(
    SECTION,
    "DoWhile_2:  " +description +" - code after loop but inside label should be executed",
    "pass",
    result2 );
}

/**
 *  While with a labeled break.
 *
 */
function DoWhile_3() {
  var description = "while with a labeled break statement";
  var result1 = "pass";
  var result2 = "pass";
  var result3 = "fail: did not get to code after label";

woohoo: {
    while( true ) {
      break woohoo;
      result1 = "fail: got to code after a break";
    }
    result2 = "fail: got to code outside of loop but inside label";
  }

  result3 = "pass";

  new TestCase(
    SECTION,
    "DoWhile_3: " +description +" - verify break out of loop",
    "pass",
    result1 );


  new TestCase(
    SECTION,
    "DoWhile_3: " +description +" - verify break out of label",
    "pass",
    result2 );

  new TestCase(
    SECTION,
    "DoWhile_3: " +description + " - verify correct exit from label",
    "pass",
    result3 );
}


/**
 *  Labled while with an unlabeled break
 *
 */
function DoWhile_4() {
  var description = "labeled while with an unlabeled break";
  var result1 = "pass";
  var result2 = "pass";
  var result3 = "fail: did not evaluate statement after label";

woohooboy: {
    while( true ) {
      break woohooboy;
      result1 = "fail: got to code after the break";
    }
    result2 = "fail: broke out of while, but not out of label";
  }
  result3 = "pass";

  new TestCase(
    SECTION,
    "DoWhile_4: " +description +" - verify break out of while loop",
    "pass",
    result1 );

  new TestCase(
    SECTION,
    "DoWhile_4: " +description + " - verify break out of label",
    "pass",
    result2 );

  new TestCase(
    SECTION,
    "DoWhile_4: " +description +" - verify that statements after label are evaluated",
    "pass",
    result3 );
}

/**
 *  in this case, should behave the same way as
 *
 *
 */
function DoWhile_5() {
  var description = "while with a labeled continue statement";
  var result1 = "pass";
  var result2 = "fail: did not execute code after loop, but inside label";
  var i = 0;
  var j = 0;

theloop: {
    j++;
    while( i++ < 10  ) {
      continue;
      result1 = "failed:  hit code after continue statement";
    }
    result2 = "pass";
  }

  new TestCase(
    SECTION,
    "DoWhile_5: " +description + " - continue should not execute statements above the loop",
    true,
    ( j == 1 ) );

  new TestCase(
    SECTION,
    "DoWhile_5: " +description +" - code after labeled continue should not be executed",
    "pass",
    result1 );

  new TestCase(
    SECTION,
    "DoWhile_5: " +description +" - code after loop but inside label should be executed",
    "pass",
    result2 );
}


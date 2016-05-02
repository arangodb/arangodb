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

gTestfile = 'dowhile-005.js';

/**
 *  File Name:          dowhile-005
 *  ECMA Section:
 *  Description:        do...while statements
 *
 *  Test a labeled do...while.  Break out of the loop with no label
 *  should break out of the loop, but not out of the label.
 *
 *  Currently causes an infinite loop in the monkey.  Uncomment the
 *  print statement below and it works OK.
 *
 *  Author:             christine@netscape.com
 *  Date:               26 August 1998
 */
var SECTION = "dowhile-005";
var VERSION = "ECMA_2";
var TITLE   = "do...while with a labeled continue statement";
var BUGNUMBER = "316293";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

NestedLabel();


test();

function NestedLabel() {
  i = 0;
  result1 = "pass";
  result2 = "fail: did not hit code after inner loop";
  result3 = "pass";

outer: {
    do {
    inner: {
//                    print( i );
	break inner;
	result1 = "fail: did break out of inner label";
      }
      result2 = "pass";
      break outer;
      print(i);
    } while ( i++ < 100 );

  }

  result3 = "fail: did not break out of outer label";

  new TestCase(
    SECTION,
    "number of loop iterations",
    0,
    i );

  new TestCase(
    SECTION,
    "break out of inner loop",
    "pass",
    result1 );

  new TestCase(
    SECTION,
    "break out of outer loop",
    "pass",
    result2 );
}

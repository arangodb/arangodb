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

gTestfile = 'dowhile-004.js';

/**
 *  File Name:          dowhile-004
 *  ECMA Section:
 *  Description:        do...while statements
 *
 *  Test a labeled do...while.  Break out of the loop with no label
 *  should break out of the loop, but not out of the label.
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "dowhile-004";
var VERSION = "ECMA_2";
var TITLE   = "do...while with a labeled continue statement";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

DoWhile( 0, 1 );
DoWhile( 1, 1 );
DoWhile( -1, 1 );
DoWhile( 5, 5 );

test();

function DoWhile( limit, expect ) {
  i = 0;
  result1 = "pass";
  result2 = "failed: broke out of labeled statement unexpectedly";

foo: {
    do {
      i++;
      if ( ! (i < limit) ) {
	break;
	result1 = "fail: evaluated statement after a labeled break";
      }
    } while ( true );

    result2 = "pass";
  }

  new TestCase(
    SECTION,
    "do while ( " + i +" < " + limit +" )",
    expect,
    i );

  new TestCase(
    SECTION,
    "breaking out of a do... while loop",
    "pass",
    result1 );


  new TestCase(
    SECTION,
    "breaking out of a labeled do...while loop",
    "pass",
    result2 );
}

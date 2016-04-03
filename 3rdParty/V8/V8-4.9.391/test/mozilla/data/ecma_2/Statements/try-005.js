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

gTestfile = 'try-005.js';

/**
 *  File Name:          try-005.js
 *  ECMA Section:
 *  Description:        The try statement
 *
 *  This test has a try with one catch block but no finally.  Same
 *  as try-004, but the eval statement is called from a function, not
 *  directly from within the try block.
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "try-005";
var VERSION = "ECMA_2";
var TITLE   = "The try statement";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

TryToCatch( "Math.PI", Math.PI );
TryToCatch( "Thrower(5)",   "Caught 5" );
TryToCatch( "Thrower(\"some random exception\")", "Caught some random exception" );

test();

function Thrower( v ) {
  throw "Caught " + v;
}
function Eval( v ) {
  return eval( v );
}

/**
 *  Evaluate a string.  Catch any exceptions thrown.  If no exception is
 *  expected, verify the result of the evaluation.  If an exception is
 *  expected, verify that we got the right exception.
 */

function TryToCatch( value, expect ) {
  try {
    result = Eval( value );
  } catch ( e ) {
    result = e;
  }

  new TestCase(
    SECTION,
    "eval( " + value +" )",
    expect,
    result );
}

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

gTestfile = 'try-001.js';

/**
 *  File Name:          try-001.js
 *  ECMA Section:
 *  Description:        The try statement
 *
 *  This test contains try, catch, and finally blocks.  An exception is
 *  sometimes thrown by a function called from within the try block.
 *
 *  This test doesn't actually make any LiveConnect calls.
 *
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "";
var VERSION = "ECMA_2";
var TITLE   = "The try statement";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

var INVALID_JAVA_INTEGER_VALUE = "Invalid value for java.lang.Integer constructor";

TryNewJavaInteger( "3.14159", INVALID_JAVA_INTEGER_VALUE );
TryNewJavaInteger( NaN, INVALID_JAVA_INTEGER_VALUE );
TryNewJavaInteger( 0,  0 );
TryNewJavaInteger( -1, -1 );
TryNewJavaInteger( 1,  1 );
TryNewJavaInteger( Infinity, Infinity );

test();

/**
 *  Check to see if the input is valid for java.lang.Integer. If it is
 *  not valid, throw INVALID_JAVA_INTEGER_VALUE.  If input is valid,
 *  return Number( v )
 *
 */

function newJavaInteger( v ) {
  value = Number( v );
  if ( Math.floor(value) != value || isNaN(value) ) {
    throw ( INVALID_JAVA_INTEGER_VALUE );
  } else {
    return value;
  }
}

/**
 *  Call newJavaInteger( value ) from within a try block.  Catch any
 *  exception, and store it in result.  Verify that we got the right
 *  return value from newJavaInteger in cases in which we do not expect
 *  exceptions, and that we got the exception in cases where an exception
 *  was expected.
 */
function TryNewJavaInteger( value, expect ) {
  var finalTest = false;

  try {
    result = newJavaInteger( value );
  } catch ( e ) {
    result = String( e );
  } finally {
    finalTest = true;
  }
  new TestCase(
    SECTION,
    "newJavaValue( " + value +" )",
    expect,
    result);

  new TestCase(
    SECTION,
    "newJavaValue( " + value +" ) hit finally block",
    true,
    finalTest);

}


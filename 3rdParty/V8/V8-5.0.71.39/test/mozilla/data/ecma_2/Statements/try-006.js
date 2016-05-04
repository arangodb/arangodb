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

gTestfile = 'try-006.js';

/**
 *  File Name:          try-006.js
 *  ECMA Section:
 *  Description:        The try statement
 *
 *  Throw an exception from within a With block in a try block.  Verify
 *  that any expected exceptions are caught.
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "try-006";
var VERSION = "ECMA_2";
var TITLE   = "The try statement";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

/**
 *  This is the "check" function for test objects that will
 *  throw an exception.
 */
function throwException() {
  throw EXCEPTION_STRING +": " + this.valueOf();
}
var EXCEPTION_STRING = "Exception thrown:";

/**
 *  This is the "check" function for test objects that do not
 *  throw an exception
 */
function noException() {
  return this.valueOf();
}

/**
 *  Add test cases here
 */
TryWith( new TryObject( "hello", throwException, true ));
TryWith( new TryObject( "hola",  noException, false ));

/**
 *  Run the test.
 */

test();

/**
 *  This is the object that will be the "this" in a with block.
 */
function TryObject( value, fun, exception ) {
  this.value = value;
  this.exception = exception;

  this.valueOf = new Function ( "return this.value" );
  this.check = fun;
}

/**
 *  This function has the try block that has a with block within it.
 *  Test cases are added in this function.  Within the with block, the
 *  object's "check" function is called.  If the test object's exception
 *  property is true, we expect the result to be the exception value.
 *  If exception is false, then we expect the result to be the value of
 *  the object.
 */
function TryWith( object ) {
  try {
    with ( object ) {
      result = check();
    }
  } catch ( e ) {
    result = e;
  }

  new TestCase(
    SECTION,
    "TryWith( " + object.value +" )",
    (object.exception ? EXCEPTION_STRING +": " + object.valueOf() : object.valueOf()),
    result );
}

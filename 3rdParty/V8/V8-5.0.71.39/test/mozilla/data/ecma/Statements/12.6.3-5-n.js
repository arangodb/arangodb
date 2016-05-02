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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
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

gTestfile = '12.6.3-5-n.js';

/**
   File Name:          12.6.3-1.js
   ECMA Section:       12.6.3 The for...in Statement
   Description:
   The production IterationStatement : for ( LeftHandSideExpression in Expression )
   Statement is evaluated as follows:

   1.  Evaluate the Expression.
   2.  Call GetValue(Result(1)).
   3.  Call ToObject(Result(2)).
   4.  Let C be "normal completion".
   5.  Get the name of the next property of Result(3) that doesn't have the
   DontEnum attribute. If there is no such property, go to step 14.
   6.  Evaluate the LeftHandSideExpression ( it may be evaluated repeatedly).
   7.  Call PutValue(Result(6), Result(5)).  PutValue( V, W ):
   1.  If Type(V) is not Reference, generate a runtime error.
   2.  Call GetBase(V).
   3.  If Result(2) is null, go to step 6.
   4.  Call the [[Put]] method of Result(2), passing GetPropertyName(V)
   for the property name and W for the value.
   5.  Return.
   6.  Call the [[Put]] method for the global object, passing
   GetPropertyName(V) for the property name and W for the value.
   7.  Return.
   8.  Evaluate Statement.
   9.  If Result(8) is a value completion, change C to be "normal completion
   after value V" where V is the value carried by Result(8).
   10. If Result(8) is a break completion, go to step 14.
   11. If Result(8) is a continue completion, go to step 5.
   12. If Result(8) is a return completion, return Result(8).
   13. Go to step 5.
   14. Return C.

   Author:             christine@netscape.com
   Date:               11 september 1997
*/
var SECTION = "12.6.3-4";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The for..in statement";

writeHeaderToLog( SECTION + " "+ TITLE);

//  for ( LeftHandSideExpression in Expression )
//  LeftHandSideExpression:NewExpression:MemberExpression

DESCRIPTION = "more than one member expression";
EXPECTED = "error";

new TestCase( SECTION,
	      "more than one member expression",
	      "error",
	      eval("var o = new MyObject(); var result = 0; for ( var i, p in this) { result += this[p]; }") );

/*
  var o = new MyObject();
  var result = 0;

  for ( var i, p in this) {
  result += this[p];
  }
*/

test();

function MyObject() {
  this.value = 2;
  this[0] = 4;
  return this;
}

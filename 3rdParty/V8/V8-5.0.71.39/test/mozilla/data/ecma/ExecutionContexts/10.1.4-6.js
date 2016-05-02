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

gTestfile = '10.1.4-6.js';

/**
   File Name:          10.1.4-1.js
   ECMA Section:       10.1.4 Scope Chain and Identifier Resolution
   Description:
   Every execution context has associated with it a scope chain. This is
   logically a list of objects that are searched when binding an Identifier.
   When control enters an execution context, the scope chain is created and
   is populated with an initial set of objects, depending on the type of
   code. When control leaves the execution context, the scope chain is
   destroyed.

   During execution, the scope chain of the execution context is affected
   only by WithStatement. When execution enters a with block, the object
   specified in the with statement is added to the front of the scope chain.
   When execution leaves a with block, whether normally or via a break or
   continue statement, the object is removed from the scope chain. The object
   being removed will always be the first object in the scope chain.

   During execution, the syntactic production PrimaryExpression : Identifier
   is evaluated using the following algorithm:

   1.  Get the next object in the scope chain. If there isn't one, go to step 5.
   2.  Call the [[HasProperty]] method of Result(l), passing the Identifier as
   the property.
   3.  If Result(2) is true, return a value of type Reference whose base object
   is Result(l) and whose property name is the Identifier.
   4.  Go to step 1.
   5.  Return a value of type Reference whose base object is null and whose
   property name is the Identifier.
   The result of binding an identifier is always a value of type Reference with
   its member name component equal to the identifier string.
   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "10.1.4-6";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Scope Chain and Identifier Resolution");


var testcase = new TestCase( "SECTION",
			     "with MyObject, eval should be [object Global].eval " );

var MYOBJECT = new MyObject();
var INPUT = 2;
testcase.description += ( INPUT +"" );

with ( MYOBJECT ) {
  ;
}
testcase.actual = eval( INPUT );
testcase.expect = INPUT;

test();


function MyObject() {
  this.eval = new Function( "x", "return(Math.pow(Number(x),2))" );
}

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

gTestfile = '11.2.2-10-n.js';

/**
   File Name:          11.2.2-9-n.js
   ECMA Section:       11.2.2. The new operator
   Description:

   MemberExpression:
   PrimaryExpression
   MemberExpression[Expression]
   MemberExpression.Identifier
   new MemberExpression Arguments

   new NewExpression

   The production NewExpression : new NewExpression is evaluated as follows:

   1.   Evaluate NewExpression.
   2.   Call GetValue(Result(1)).
   3.   If Type(Result(2)) is not Object, generate a runtime error.
   4.   If Result(2) does not implement the internal [[Construct]] method,
   generate a runtime error.
   5.   Call the [[Construct]] method on Result(2), providing no arguments
   (that is, an empty list of arguments).
   6.   If Type(Result(5)) is not Object, generate a runtime error.
   7.   Return Result(5).

   The production MemberExpression : new MemberExpression Arguments is evaluated as follows:

   1.   Evaluate MemberExpression.
   2.   Call GetValue(Result(1)).
   3.   Evaluate Arguments, producing an internal list of argument values
   (section 0).
   4.   If Type(Result(2)) is not Object, generate a runtime error.
   5.   If Result(2) does not implement the internal [[Construct]] method,
   generate a runtime error.
   6.   Call the [[Construct]] method on Result(2), providing the list
   Result(3) as the argument values.
   7.   If Type(Result(6)) is not Object, generate a runtime error.
   8    .Return Result(6).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "11.2.2-9-n.js";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The new operator";

writeHeaderToLog( SECTION + " "+ TITLE);

DESCRIPTION = "var m = new Math()";
EXPECTED = "error";

new TestCase( SECTION,
              "var m = new Math()",
              "error",
              eval("m = new Math()") );
test();

function TestFunction() {
  return arguments;
}

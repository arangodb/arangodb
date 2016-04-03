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

gTestfile = '11.2.3-3-n.js';

/**
   File Name:          11.2.3-3-n.js
   ECMA Section:       11.2.3. Function Calls
   Description:

   The production CallExpression : MemberExpression Arguments is evaluated as
   follows:

   1.Evaluate MemberExpression.
   2.Evaluate Arguments, producing an internal list of argument values
   (section 0).
   3.Call GetValue(Result(1)).
   4.If Type(Result(3)) is not Object, generate a runtime error.
   5.If Result(3) does not implement the internal [[Call]] method, generate a
   runtime error.
   6.If Type(Result(1)) is Reference, Result(6) is GetBase(Result(1)). Otherwise,
   Result(6) is null.
   7.If Result(6) is an activation object, Result(7) is null. Otherwise, Result(7) is
   the same as Result(6).
   8.Call the [[Call]] method on Result(3), providing Result(7) as the this value
   and providing the list Result(2) as the argument values.
   9.Return Result(8).

   The production CallExpression : CallExpression Arguments is evaluated in
   exactly the same manner, except that the contained CallExpression is
   evaluated in step 1.

   Note: Result(8) will never be of type Reference if Result(3) is a native
   ECMAScript object. Whether calling a host object can return a value of
   type Reference is implementation-dependent.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "11.2.3-3-n.js";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Function Calls";

writeHeaderToLog( SECTION + " "+ TITLE);

DESCRIPTION = "(void 0).valueOf()";
EXPECTED = "error";

new TestCase( SECTION,
              "(void 0).valueOf()",
              "error",
              eval("(void 0).valueOf()") );
test();


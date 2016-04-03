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

gTestfile = '12.2-1.js';

/**
   File Name:          12.2-1.js
   ECMA Section:       The variable statement
   Description:

   If the variable statement occurs inside a FunctionDeclaration, the
   variables are defined with function-local scope in that function, as
   described in section 10.1.3. Otherwise, they are defined with global
   scope, that is, they are created as members of the global object, as
   described in section 0. Variables are created when the execution scope
   is entered. A Block does not define a new execution scope. Only Program and
   FunctionDeclaration produce a new scope. Variables are initialized to the
   undefined value when created. A variable with an Initializer is assigned
   the value of its AssignmentExpression when the VariableStatement is executed,
   not when the variable is created.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "12.2-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The variable statement";

writeHeaderToLog( SECTION +" "+ TITLE);

new TestCase(    "SECTION",
		 "var x = 3; function f() { var a = x; var x = 23; return a; }; f()",
		 void 0,
		 eval("var x = 3; function f() { var a = x; var x = 23; return a; }; f()") );

test();


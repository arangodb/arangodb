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

gTestfile = '11.1.1.js';

/**
   File Name:          11.1.1.js
   ECMA Section:       11.1.1 The this keyword
   Description:

   The this keyword evaluates to the this value of the execution context.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.1.1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " The this keyword");

var GLOBAL_OBJECT = this.toString();

// this in global code and eval(this) in global code should return the global object.

new TestCase( SECTION,
	      "Global Code: this.toString()",
	      GLOBAL_OBJECT,
	      this.toString() );

new TestCase( SECTION,
	      "Global Code:  eval('this.toString()')",
	      GLOBAL_OBJECT,
	      eval('this.toString()') );

// this in anonymous code called as a function should return the global object.

new TestCase( SECTION,
	      "Anonymous Code: var MYFUNC = new Function('return this.toString()'); MYFUNC()",
	      GLOBAL_OBJECT,
	      eval("var MYFUNC = new Function('return this.toString()'); MYFUNC()") );

// eval( this ) in anonymous code called as a function should return that function's activation object

new TestCase( SECTION,
	      "Anonymous Code: var MYFUNC = new Function('return (eval(\"this.toString()\")'); (MYFUNC()).toString()",
	      GLOBAL_OBJECT,
	      eval("var MYFUNC = new Function('return eval(\"this.toString()\")'); (MYFUNC()).toString()") );

// this and eval( this ) in anonymous code called as a constructor should return the object

new TestCase( SECTION,
	      "Anonymous Code: var MYFUNC = new Function('this.THIS = this'); ((new MYFUNC()).THIS).toString()",
	      "[object Object]",
	      eval("var MYFUNC = new Function('this.THIS = this'); ((new MYFUNC()).THIS).toString()") );

new TestCase( SECTION,
	      "Anonymous Code: var MYFUNC = new Function('this.THIS = this'); var FUN1 = new MYFUNC(); FUN1.THIS == FUN1",
	      true,
	      eval("var MYFUNC = new Function('this.THIS = this'); var FUN1 = new MYFUNC(); FUN1.THIS == FUN1") );

new TestCase( SECTION,
	      "Anonymous Code: var MYFUNC = new Function('this.THIS = eval(\"this\")'); ((new MYFUNC().THIS).toString()",
	      "[object Object]",
	      eval("var MYFUNC = new Function('this.THIS = eval(\"this\")'); ((new MYFUNC()).THIS).toString()") );

new TestCase( SECTION,
	      "Anonymous Code: var MYFUNC = new Function('this.THIS = eval(\"this\")'); var FUN1 = new MYFUNC(); FUN1.THIS == FUN1",
	      true,
	      eval("var MYFUNC = new Function('this.THIS = eval(\"this\")'); var FUN1 = new MYFUNC(); FUN1.THIS == FUN1") );

// this and eval(this) in function code called as a function should return the global object.
new TestCase( SECTION,
	      "Function Code:  ReturnThis()",
	      GLOBAL_OBJECT,
	      ReturnThis() );

new TestCase( SECTION,
	      "Function Code:  ReturnEvalThis()",
	      GLOBAL_OBJECT,
	      ReturnEvalThis() );

//  this and eval(this) in function code called as a contructor should return the object.
new TestCase( SECTION,
	      "var MYOBJECT = new ReturnThis(); MYOBJECT.toString()",
	      "[object Object]",
	      eval("var MYOBJECT = new ReturnThis(); MYOBJECT.toString()") );

new TestCase( SECTION,
	      "var MYOBJECT = new ReturnEvalThis(); MYOBJECT.toString()",
	      "[object Object]",
	      eval("var MYOBJECT = new ReturnEvalThis(); MYOBJECT.toString()") );

test();

function ReturnThis() {
  return this.toString();
}

function ReturnEvalThis() {
  return( eval("this.toString()") );
}

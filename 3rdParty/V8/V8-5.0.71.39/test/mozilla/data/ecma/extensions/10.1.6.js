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

gTestfile = '10.1.6.js';

/**
   File Name:          10.1.6
   ECMA Section:       Activation Object
   Description:

   If the function object being invoked has an arguments property, let x be
   the value of that property; the activation object is also given an internal
   property [[OldArguments]] whose initial value is x; otherwise, an arguments
   property is created for the function object but the activation object is
   not given an [[OldArguments]] property. Next, arguments object described
   below (the same one stored in the arguments property of the activation
   object) is used as the new value of the arguments property of the function
   object. This new value is installed even if the arguments property already
   exists and has the ReadOnly attribute (as it will for native Function
   objects). (These actions are taken to provide compatibility with a form of
   program syntax that is now discouraged: to access the arguments object for
   function f within the body of f by using the expression f.arguments.
   The recommended way to access the arguments object for function f within
   the body of f is simply to refer to the variable arguments.)

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "10.1.6";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Activation Object";

writeHeaderToLog( SECTION + " "+ TITLE);

var arguments = "FAILED!";

var ARG_STRING = "value of the argument property";

new TestCase( SECTION,
	      "(new TestObject(0,1,2,3,4,5)).length",
	      6,
	      (new TestObject(0,1,2,3,4,5)).length );

for ( i = 0; i < 6; i++ ) {

  new TestCase( SECTION,
		"(new TestObject(0,1,2,3,4,5))["+i+"]",
		i,
		(new TestObject(0,1,2,3,4,5))[i]);
}


//    The current object already has an arguments property.

new TestCase( SECTION,
	      "(new AnotherTestObject(1,2,3)).arguments",
	      ARG_STRING,
	      (new AnotherTestObject(1,2,3)).arguments );

//  The function invoked with [[Call]]

new TestCase( SECTION,
	      "TestFunction(1,2,3)",
	      ARG_STRING,
	      TestFunction() + '' );


test();



function Prototype() {
  this.arguments = ARG_STRING;
}
function TestObject() {
  this.__proto__ = new Prototype();
  return arguments;
}
function AnotherTestObject() {
  this.__proto__ = new Prototype();
  return this;
}
function TestFunction() {
  arguments = ARG_STRING;
  return arguments;
}
function AnotherTestFunction() {
  this.__proto__ = new Prototype();
  return this;
}

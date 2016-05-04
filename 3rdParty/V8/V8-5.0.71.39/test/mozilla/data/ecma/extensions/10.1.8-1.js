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

gTestfile = '10.1.8-1.js';

/**
   File Name:          10.1.8
   ECMA Section:       Arguments Object
   Description:

   When control enters an execution context for declared function code,
   anonymous code, or implementation-supplied code, an arguments object is
   created and initialized as follows:

   The [[Prototype]] of the arguments object is to the original Object
   prototype object, the one that is the initial value of Object.prototype
   (section 15.2.3.1).

   A property is created with name callee and property attributes {DontEnum}.
   The initial value of this property is the function object being executed.
   This allows anonymous functions to be recursive.

   A property is created with name length and property attributes {DontEnum}.
   The initial value of this property is the number of actual parameter values
   supplied by the caller.

   For each non-negative integer, iarg, less than the value of the length
   property, a property is created with name ToString(iarg) and property
   attributes { DontEnum }. The initial value of this property is the value
   of the corresponding actual parameter supplied by the caller. The first
   actual parameter value corresponds to iarg = 0, the second to iarg = 1 and
   so on. In the case when iarg is less than the number of formal parameters
   for the function object, this property shares its value with the
   corresponding property of the activation object. This means that changing
   this property changes the corresponding property of the activation object
   and vice versa. The value sharing mechanism depends on the implementation.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "10.1.8";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Arguments Object";

writeHeaderToLog( SECTION + " "+ TITLE);

var ARG_STRING = "value of the argument property";

new TestCase( SECTION,
	      "GetCallee()",
	      GetCallee,
	      GetCallee() );

var LIMIT = 100;

for ( var i = 0, args = "" ; i < LIMIT; i++ ) {
  args += String(i) + ( i+1 < LIMIT ? "," : "" );

}

var LENGTH = eval( "GetLength("+ args +")" );

new TestCase( SECTION,
	      "GetLength("+args+")",
	      100,
	      LENGTH );

var ARGUMENTS = eval( "GetArguments( " +args+")" );

for ( var i = 0; i < 100; i++ ) {
  new TestCase( SECTION,
		"GetArguments("+args+")["+i+"]",
		i,
		ARGUMENTS[i] );
}

test();

function TestFunction() {
  var arg_proto = arguments.__proto__;
}
function GetCallee() {
  var c = arguments.callee;
  return c;
}
function GetArguments() {
  var a = arguments;
  return a;
}
function GetLength() {
  var l = arguments.length;
  return l;
}

function AnotherTestFunction() {
  this.__proto__ = new Prototype();
  return this;
}

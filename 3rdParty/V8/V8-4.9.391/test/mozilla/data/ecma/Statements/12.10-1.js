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

gTestfile = '12.10-1.js';

/**
   File Name:          12.10-1.js
   ECMA Section:       12.10 The with statement
   Description:
   WithStatement :
   with ( Expression ) Statement

   The with statement adds a computed object to the front of the scope chain
   of the current execution context, then executes a statement with this
   augmented scope chain, then restores the scope chain.

   Semantics

   The production WithStatement : with ( Expression ) Statement is evaluated
   as follows:
   1.  Evaluate Expression.
   2.  Call GetValue(Result(1)).
   3.  Call ToObject(Result(2)).
   4.  Add Result(3) to the front of the scope chain.
   5.  Evaluate Statement using the augmented scope chain from step 4.
   6.  Remove Result(3) from the front of the scope chain.
   7.  Return Result(5).

   Discussion
   Note that no matter how control leaves the embedded Statement, whether
   normally or by some form of abrupt completion, the scope chain is always
   restored to its former state.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "12.10-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The with statement";

writeHeaderToLog( SECTION + " "+ TITLE);


// although the scope chain changes, the this value is immutable for a given
// execution context.

new TestCase( SECTION,
	      "with( new Number() ) { this +'' }",
	      GLOBAL,
	      eval("with( new Number() ) { this +'' }") );

// the object's functions and properties should override those of the
// global object.

new TestCase(
  SECTION,
  "var MYOB = new WithObject(true); with (MYOB) { parseInt() }",
  true,
  eval("var MYOB = new WithObject(true); with (MYOB) { parseInt() }") );

new TestCase(
  SECTION,
  "var MYOB = new WithObject(false); with (MYOB) { NaN }",
  false,
  eval("var MYOB = new WithObject(false); with (MYOB) { NaN }") );

new TestCase(
  SECTION,
  "var MYOB = new WithObject(NaN); with (MYOB) { Infinity }",
  Number.NaN,
  eval("var MYOB = new WithObject(NaN); with (MYOB) { Infinity }") );

new TestCase(
  SECTION,
  "var MYOB = new WithObject(false); with (MYOB) { }; Infinity",
  Number.POSITIVE_INFINITY,
  eval("var MYOB = new WithObject(false); with (MYOB) { }; Infinity") );


new TestCase(
  SECTION,
  "var MYOB = new WithObject(0); with (MYOB) { delete Infinity; Infinity }",
  Number.POSITIVE_INFINITY,
  eval("var MYOB = new WithObject(0); with (MYOB) { delete Infinity; Infinity }") );

// let us leave the with block via a break.

new TestCase(
  SECTION,
  "var MYOB = new WithObject(0); while (true) { with (MYOB) { Infinity; break; } } Infinity",
  Number.POSITIVE_INFINITY,
  eval("var MYOB = new WithObject(0); while (true) { with (MYOB) { Infinity; break; } } Infinity") );


test();

function WithObject( value ) {
  this.prop1 = 1;
  this.prop2 = new Boolean(true);
  this.prop3 = "a string";
  this.value = value;

  // now we will override global functions

  this.parseInt = new Function( "return this.value" );
  this.NaN = value;
  this.Infinity = value;
  this.unescape = new Function( "return this.value" );
  this.escape   = new Function( "return this.value" );
  this.eval     = new Function( "return this.value" );
  this.parseFloat = new Function( "return this.value" );
  this.isNaN      = new Function( "return this.value" );
  this.isFinite   = new Function( "return this.value" );
}

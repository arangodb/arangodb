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

gTestfile = '8.6.2.1-1.js';

/**
   File Name:          8.6.2.1-1.js
   ECMA Section:       8.6.2.1 Get (Value)
   Description:

   When the [[Get]] method of O is called with property name P, the following
   steps are taken:

   1.  If O doesn't have a property with name P, go to step 4.
   2.  Get the value of the property.
   3.  Return Result(2).
   4.  If the [[Prototype]] of O is null, return undefined.
   5.  Call the [[Get]] method of [[Prototype]] with property name P.
   6.  Return Result(5).

   This tests [[Get]] (Value).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "8.6.2.1-1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " [[Get]] (Value)");

new TestCase( SECTION,  "var OBJ = new MyValuelessObject(true); OBJ.valueOf()",     true,           eval("var OBJ = new MyValuelessObject(true); OBJ.valueOf()") );
//    new TestCase( SECTION,  "var OBJ = new MyProtoValuelessObject(true); OBJ + ''",     "undefined",    eval("var OBJ = new MyProtoValuelessObject(); OBJ + ''") );
new TestCase( SECTION,  "var OBJ = new MyProtolessObject(true); OBJ.valueOf()",     true,           eval("var OBJ = new MyProtolessObject(true); OBJ.valueOf()") );

new TestCase( SECTION,  "var OBJ = new MyValuelessObject(Number.POSITIVE_INFINITY); OBJ.valueOf()",     Number.POSITIVE_INFINITY,           eval("var OBJ = new MyValuelessObject(Number.POSITIVE_INFINITY); OBJ.valueOf()") );
//    new TestCase( SECTION,  "var OBJ = new MyProtoValuelessObject(Number.POSITIVE_INFINITY); OBJ + ''",     "undefined",                        eval("var OBJ = new MyProtoValuelessObject(); OBJ + ''") );
new TestCase( SECTION,  "var OBJ = new MyProtolessObject(Number.POSITIVE_INFINITY); OBJ.valueOf()",     Number.POSITIVE_INFINITY,           eval("var OBJ = new MyProtolessObject(Number.POSITIVE_INFINITY); OBJ.valueOf()") );

new TestCase( SECTION,  "var OBJ = new MyValuelessObject('string'); OBJ.valueOf()",     'string',           eval("var OBJ = new MyValuelessObject('string'); OBJ.valueOf()") );
//    new TestCase( SECTION,  "var OBJ = new MyProtoValuelessObject('string'); OJ + ''",     "undefined",      eval("var OBJ = new MyProtoValuelessObject(); OBJ + ''") );
new TestCase( SECTION,  "var OBJ = new MyProtolessObject('string'); OBJ.valueOf()",     'string',           eval("var OBJ = new MyProtolessObject('string'); OBJ.valueOf()") );

test();

function MyProtoValuelessObject(value) {
  this.valueOf = new Function ( "" );
  this.__proto__ = null;
}

function MyProtolessObject( value ) {
  this.valueOf = new Function( "return this.value" );
  this.__proto__ = null;
  this.value = value;
}
function MyValuelessObject(value) {
  this.__proto__ = new MyPrototypeObject(value);
}
function MyPrototypeObject(value) {
  this.valueOf = new Function( "return this.value;" );
  this.toString = new Function( "return (this.value + '');" );
  this.value = value;
}

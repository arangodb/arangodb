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

gTestfile = 'object-005.js';

/**
   File Name:      object-005.js
   Description:

   Call ToNumber, ToString, and use the addition operator to
   access the DefaultValue with hints Number, String, and no
   hint (respectively).

   @author     christine@netscape.com
   @version    1.00
*/
var SECTION = "LiveConnect Objects";
var VERSION = "1_3";
var TITLE   = "Getting the Class of JavaObjects";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

var a = new Array();
var i = 0;

// here's an object that should be converted to a number
a[i++] = new TestObject( "new java.lang.Integer(999)",
			 new java.lang.Integer(999), "Number", 999, "number" );

a[i++] = new TestObject(
  "new java.lang.Float(999.0)",
  new java.lang.Float(999.0),
  "Number",
  999,
  "number" );

a[i++] = new TestObject(
  "new java.lang.String(\"hi\")",
  new java.lang.String("hi"),
  "String",
  "hi",
  "string" );

a[i++] = new TestObject(
  "new java.lang.Integer(2134)",
  new java.lang.Integer(2134),
  "0 + ",
  "21340",
  "string" );

a[i++] = new TestObject(
  "new java.lang.Integer(666)",
  new java.lang.Integer(666),
  "Boolean",
  true,
  "boolean" );

for ( i = 0; i < a.length; i++ ) {
  CompareValues( a[i] );
}

test();

function CompareValues( t ) {
  new TestCase(
    SECTION,
    t.converter +"("+ t.description +")",
    t.expect,
    t.actual );

  new TestCase(
    SECTION,
    "typeof (" + t.converter +"( "+ t.description +" ) )",
    t.type,
    typeof t.actual );
}
function TestObject( description, javaobject, converter, expect, type ) {
  this.description = description;
  this.javavalue = javaobject
    this.converter = converter;
  this.expect = expect;
  this.type = type;

  switch ( converter ) {
  case( "Number" ) :  this.actual = Number( javaobject ); break;
  case( "String" ) :  this.actual = String( javaobject ); break;
  case( "Boolean") :  this.actual = Boolean(javaobject ); break;
  default:            this.actual = javaobject + 0;
  }
  return this;
}

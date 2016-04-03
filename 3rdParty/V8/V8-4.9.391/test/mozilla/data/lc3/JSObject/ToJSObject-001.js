/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */
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

gTestfile = 'ToJSObject-001.js';

/**
 *  JavaScript to Java type conversion.
 *
 *  This test passes JavaScript number values to several Java methods
 *  that expect arguments of various types, and verifies that the value is
 *  converted to the correct value and type.
 *
 *  This tests instance methods, and not static methods.
 *
 *  Running these tests successfully requires you to have
 *  com.netscape.javascript.qa.liveconnect.DataTypeClass on your classpath.
 *
 *  Specification:  Method Overloading Proposal for Liveconnect 3.0
 *
 *  @author: christine@netscape.com
 *
 */
var SECTION = "JavaScript Object to java.lang.String";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var jsoc = new Packages.com.netscape.javascript.qa.liveconnect.JSObjectConversion();

var a = new Array();
var i = 0;

// 3.3.6.4 Other JavaScript Objects
// Passing a JavaScript object to a java method that that expects a JSObject
// should wrap the JS object in a new instance of netscape.javascript.JSObject.
// HOwever, since we are running the test from JavaScript, getting the value
// back should return the unwrapped object.

var string  = new String("JavaScript String Value");

a[i++] = new TestObject(
  "jsoc.setJSObject(string)",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  string,
  String);

var myobject = new MyObject( string );

a[i++] = new TestObject(
  "jsoc.setJSObject( myobject )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  myobject,
  MyObject);

var bool = new Boolean(true);

a[i++] = new TestObject(
  "jsoc.setJSObject( bool )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  bool,
  Boolean);

bool = new Boolean(false);

a[i++] = new TestObject(
  "jsoc.setJSObject( bool )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  bool,
  Boolean);

var object = new Object();

a[i++] = new TestObject(
  "jsoc.setJSObject( object)",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  object,
  Object);

var number = new Number(0);

a[i++] = new TestObject(
  "jsoc.setJSObject( number )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  number,
  Number);

nan = new Number(NaN);

a[i++] = new TestObject(
  "jsoc.setJSObject( nan )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  nan,
  Number);

infinity = new Number(Infinity);

a[i++] = new TestObject(
  "jsoc.setJSObject( infinity )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  infinity,
  Number);

var neg_infinity = new Number(-Infinity);

a[i++] = new TestObject(
  "jsoc.setJSObject( neg_infinity)",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  neg_infinity,
  Number);

var array = new Array(1,2,3)

  a[i++] = new TestObject(
    "jsoc.setJSObject(array)",
    "jsoc.PUB_JSOBJECT",
    "jsoc.getJSObject()",
    "jsoc.getJSObject().constructor",
    array,
    Array);


a[i++] = new TestObject(
  "jsoc.setJSObject( MyObject )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  MyObject,
  Function);

var THIS = this;

a[i++] = new TestObject(
  "jsoc.setJSObject( THIS )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  this,
  Object);

a[i++] = new TestObject(
  "jsoc.setJSObject( Math )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  Math,
  Object);

a[i++] = new TestObject(
  "jsoc.setJSObject( Function )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  Function,
  Function);

for ( i = 0; i < a.length; i++ ) {
  new TestCase(
    a[i].description +"; "+ a[i].javaFieldName,
    a[i].jsValue,
    a[i].javaFieldValue );

  new TestCase(
    a[i].description +"; " + a[i].javaMethodName,
    a[i].jsValue,
    a[i].javaMethodValue );

  new TestCase(
    a[i].javaTypeName,
    a[i].jsType,
    a[i].javaTypeValue );
}

test();

function MyObject( stringValue ) {
  this.stringValue = String(stringValue);
  this.toString = new Function( "return this.stringValue" );
}


function TestObject( description, javaField, javaMethod, javaType,
		     jsValue, jsType )
{
  print("hi");
  eval (description);
  print("bye")
    this.description = description;
  this.javaFieldName = javaField;
  this.javaFieldValue = eval( javaField );
  this.javaMethodName = javaMethod;
  this.javaMethodValue = eval( javaMethod );
  this.javaTypeName = javaType,
    this.javaTypeValue = eval( javaType );

  this.jsValue   = jsValue;
  this.jsType      = jsType;
}

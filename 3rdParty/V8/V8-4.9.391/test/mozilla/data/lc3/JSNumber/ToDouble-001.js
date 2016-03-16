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

gTestfile = 'ToDouble-001.js';

/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

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
var SECTION = "number conversion";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var dt = new DT();

var a = new Array();
var i = 0;

// passing a number to a java method that expects a double should
// transfer the exact value to java with no rounding or loss of magnitude
// or sign.

// Special cases:  0, -0, Infinity, -Infinity, and NaN

a[i++] = new TestObject(
  "dt.setDouble( 0 )",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  0,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble( -0 )",
  "Infinity / dt.PUB_DOUBLE",
  "Infinity / dt.getDouble()",
  "typeof dt.getDouble()",
  -Infinity,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble( Infinity )",
  "dt.PUB_DOUBLE ",
  "dt.getDouble() ",
  "typeof dt.getDouble()",
  Infinity,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble( -Infinity )",
  "dt.PUB_DOUBLE",
  "dt.getDouble() ",
  "typeof dt.getDouble()",
  -Infinity,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble( NaN )",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  NaN,
  "number" );

// test cases from waldemar

a[i++] = new TestObject(
  "dt.setDouble(077777777777777777)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  2251799813685247,
  "number" );


a[i++] = new TestObject(
  "dt.setDouble(077777777777777776)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  2251799813685246,
  "number" );


a[i++] = new TestObject(
  "dt.setDouble(0x1fffffffffffff)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  9007199254740991,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(0x20000000000000)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  9007199254740992,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(0x20123456789abc)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  9027215253084860,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(0x20123456789abd)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  9027215253084860,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(0x20123456789abe)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  9027215253084862,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(0x20123456789abf)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  9027215253084864,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(0x1000000000000080)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  1152921504606847000,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(0x1000000000000081)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  1152921504606847200,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(0x1000000000000100)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  1152921504606847200,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(0x100000000000017f)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  1152921504606847200,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(0x1000000000000180)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  1152921504606847500,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(0x1000000000000181)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  1152921504606847500,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(1.7976931348623157E+308)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  1.7976931348623157e+308,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(1.7976931348623158e+308)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  1.7976931348623157e+308,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(1.7976931348623159e+308)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  Infinity,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(-1.7976931348623157E+308)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  -1.7976931348623157e+308,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(-1.7976931348623158e+308)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  -1.7976931348623157e+308,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(-1.7976931348623159e+308)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  -Infinity,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(1e-2000)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  0,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(1e2000)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  1e2000,
  "number" );

a[i++] = new TestObject(
  "dt.setDouble(-1e2000)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  -1e2000,
  "number" );

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

function TestObject( description, javaField, javaMethod, javaType,
		     jsValue, jsType )
{
  eval (description );

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

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

gTestfile = 'boolean-005.js';

/**
   File Name:      boolean-005.js
   Description:

   A java.lang.Boolean object should be read as a JavaScript JavaObject.

   @author     christine@netscape.com
   @version    1.00
*/
var SECTION = "LiveConnect";
var VERSION = "1_3";
var TITLE   = "Java Boolean Object to JavaScript Object";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

//  In all test cases, the expected type is "object"

var E_TYPE = "object";

//  The JavaScrpt [[Class]] of a JavaObject should be JavaObject"

var E_JSCLASS = "[object JavaObject]";

//  The Java class of this object is java.lang.Boolean.

var E_JAVACLASS = java.lang.Class.forName( "java.lang.Boolean" );

//  Create arrays of actual results (java_array) and expected results
//  (test_array).

var java_array = new Array();
var test_array = new Array();

var i = 0;

// Test for java.lang.Boolean.FALSE, which is a Boolean object.
java_array[i] = new JavaValue(  java.lang.Boolean.FALSE  );
test_array[i] = new TestValue(  "java.lang.Boolean.FALSE",
				false );

i++;

// Test for java.lang.Boolean.TRUE, which is a Boolean object.
java_array[i] = new JavaValue(  java.lang.Boolean.TRUE  );
test_array[i] = new TestValue(  "java.lang.Boolean.TRUE",
				true );

i++;

for ( i = 0; i < java_array.length; i++ ) {
  CompareValues( java_array[i], test_array[i] );

}

test();

function CompareValues( javaval, testval ) {
  //  Check booleanValue()
  new TestCase( SECTION,
		"("+testval.description+").booleanValue()",
		testval.value,
		javaval.value );
  //  Check typeof, which should be E_TYPE
  new TestCase( SECTION,
		"typeof (" + testval.description +")",
		testval.type,
		javaval.type );
/*
//  Check JavaScript class, which should be E_JSCLASS
new TestCase( SECTION,
"(" + testval.description +").getJSClass()",
testval.jsclass,
javaval.jsclass );
*/
  //  Check Java class, which should equal() E_JAVACLASS
  new TestCase( SECTION,
		"(" + testval.description +").getClass().equals( " + E_JAVACLASS +" )",
		true,
		javaval.javaclass.equals( testval.javaclass ) );

  // Check string representation
  new TestCase( SECTION,
		"("+ testval.description+") + ''",
		testval.string,
		javaval.string );
}
function JavaValue( value ) {
  //  java.lang.Object.getClass() returns the Java Object's class.
  this.javaclass = value.getClass();


//  __proto__ of Java objects is not supported in LC2.
// Object.prototype.toString will show its JavaScript wrapper object.
//    value.__proto__.getJSClass = Object.prototype.toString;
//    this.jsclass = value.getJSClass();

  this.string = value + "";
  print( this.string );
  this.value  = value.booleanValue();
  this.type   = typeof value;

  return this;
}
function TestValue( description, value ) {
  this.description = description;
  this.string = String( value );
  this.value = value;
  this.type =  E_TYPE;
  this.javaclass = E_JAVACLASS;
  this.jsclass = E_JSCLASS;
  return this;
}

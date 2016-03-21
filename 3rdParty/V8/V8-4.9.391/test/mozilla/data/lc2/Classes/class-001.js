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

gTestfile = 'class-001.js';

/**
   File Name:      class-001.js
   Description:

   @author     christine@netscape.com
   @version    1.00
*/
var SECTION = "LiveConnect Classes";
var VERSION = "1_3";
var TITLE   = "JavaClass objects";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

//  All packages are of the type "function", since they implement [[Construct]]
// and [[Call]]

var E_TYPE = "function";

//  The JavaScript [[Class]] property for all Classes is "[object JavaClass]"
var E_JSCLASS = "[object JavaClass]";

//  Create arrays of actual results (java_array) and
//  expected results (test_array).

var java_array = new Array();
var test_array = new Array();

var i = 0;

java_array[i] = new JavaValue(  java.awt.Image  );
test_array[i] = new TestValue(  "java.awt.Image" );
i++;

java_array[i] = new JavaValue(  java.beans.Beans  );
test_array[i] = new TestValue(  "java.beans.Beans" );
i++;

java_array[i] = new JavaValue(  java.io.File  );
test_array[i] = new TestValue(  "java.io.File" );
i++;

java_array[i] = new JavaValue(  java.lang.String  );
test_array[i] = new TestValue(  "java.lang.String" );
i++;

java_array[i] = new JavaValue(  java.math.BigDecimal  );
test_array[i] = new TestValue(  "java.math.BigDecimal" );
i++;

java_array[i] = new JavaValue(  java.net.URL  );
test_array[i] = new TestValue(  "java.net.URL" );
i++;

java_array[i] = new JavaValue(  java.text.DateFormat  );
test_array[i] = new TestValue(  "java.text.DateFormat" );
i++;

java_array[i] = new JavaValue(  java.util.Vector  );
test_array[i] = new TestValue(  "java.util.Vector" );
i++;
/*
  java_array[i] = new JavaValue(  Packages.com.netscape.javascript.Context  );
  test_array[i] = new TestValue(  "Packages.com.netscape.javascript.Context" );
  i++;
*/
for ( i = 0; i < java_array.length; i++ ) {
  CompareValues( java_array[i], test_array[i] );
}

test();
function CompareValues( javaval, testval ) {
  //  Check type, which should be E_TYPE
  new TestCase( SECTION,
		"typeof (" + testval.description +")",
		testval.type,
		javaval.type );
/*
//  Check JavaScript class, which should be E_JSCLASS
new TestCase( SECTION,
"(" + testval.description +".)getJSClass()",
testval.jsclass,
javaval.jsclass );
*/
  // Check the class's name, which should be the description, minus the "Package." part.
  new TestCase( SECTION,
		"(" + testval.description +") +''",
		testval.classname,
		javaval.classname );
}
function JavaValue( value ) {
  // Object.prototype.toString will show its JavaScript wrapper object.
//    value.__proto__.getJSClass = Object.prototype.toString;
//    this.jsclass = value.getJSClass();

  this.classname = value +"";
  this.type   = typeof value;
  return this;
}
function TestValue( description, value ) {
  this.description = description;
  this.type =  E_TYPE;
  this.jclass = E_JSCLASS;
  this.lcclass = java.lang.Class.forName( description );

  this.classname = "[JavaClass " +
    (  ( description.substring(0,9) == "Packages." )
       ? description.substring(9,description.length)
       : description
      ) + "]"


    return this;
}

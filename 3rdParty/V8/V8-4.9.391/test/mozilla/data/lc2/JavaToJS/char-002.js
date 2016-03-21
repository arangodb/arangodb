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

gTestfile = 'char-002.js';

/**
   File Name:      char-002.js
   Description:

   Java fields whose value is char should be read as a JavaScript number.

   To test this:

   1.  Instantiate a Java object that has fields with char values,
   or reference a classes static field whose value is a char.
   2.  Reference the field, and set the value of a JavaScript variable
   to that field's value.
   3.  Check the value of the returned type, which should be "number"
   4.  Check the type of the returned type, which should be a number

   It is an error if the JavaScript variable is an object, or JavaObject
   whose class is java.lang.Character.

   @author     christine@netscape.com
   @version    1.00
*/
var SECTION = "LiveConnect";
var VERSION = "1_3";
var TITLE   = "Java char return value to JavaScript Object";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

//  In all cases, the expected type is "number"
var E_TYPE = "number";

//  Create arrays of actual results (java_array) and expected results
//  (test_array).

var java_array = new Array();
var test_array = new Array();

var i = 0;

// Get File.separator char

var os = java.lang.System.getProperty( "os.name" );
var v;

if ( os.startsWith( "Windows" ) ) {
  v = 92;
} else {
  if ( os.startsWith( "Mac" ) ) {
    v = 58;
  } else {
    v = 47;
  }
}

java_array[i] = new JavaValue(  java.io.File.separatorChar   );
test_array[i] = new TestValue(  "java.io.File.separatorChar", v );

i++;

for ( i = 0; i < java_array.length; i++ ) {
  CompareValues( java_array[i], test_array[i] );

}

test();

function CompareValues( javaval, testval ) {
  //  Check value
  new TestCase( SECTION,
		testval.description,
		testval.value,
		javaval.value );
  //  Check type, which should be E_TYPE
  new TestCase( SECTION,
		"typeof (" + testval.description +")",
		testval.type,
		javaval.type );

}
function JavaValue( value ) {
  this.value  = value;
  this.type   = typeof value;
  return this;
}
function TestValue( description, value ) {
  this.description = description;
  this.value = value;
  this.type =  E_TYPE;
  return this;
}

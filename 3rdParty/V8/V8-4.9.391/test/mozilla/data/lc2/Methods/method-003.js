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

gTestfile = 'method-003.js';

/**
   File Name:      method-003.js
   Description:

   JavaMethod objects are of the type "function" since they implement the
   [[Call]] method.


   @author     christine@netscape.com
   @version    1.00
*/
var SECTION = "LiveConnect Objects";
var VERSION = "1_3";
var TITLE   = "Type and Class of Java Methods";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

//  All JavaMethods are of the type "function"
var E_TYPE = "function";

//  All JavaMethods [[Class]] property is Function
var E_JSCLASS = "[object Function]";

//  Create arrays of actual results (java_array) and
//  expected results (test_array).

var java_array = new Array();
var test_array = new Array();

var i = 0;

java_array[i] = new JavaValue(  java.lang.System.out.println  );
test_array[i] = new TestValue(  "java.lang.System.out.println" );
i++;

for ( i = 0; i < java_array.length; i++ ) {
  CompareValues( java_array[i], test_array[i] );
}

test();

function CompareValues( javaval, testval ) {
  //  Check type, which should be E_TYPE
  new TestCase( SECTION,
		"typeof (" + testval.description +" )",
		testval.type,
		javaval.type );

  //  Check JavaScript class, which should be E_JSCLASS
  new TestCase( SECTION,
		"(" + testval.description +" ).getJSClass()",
		testval.jsclass,
		javaval.jsclass );
}
function JavaValue( value ) {
  this.type   = typeof value;
  // Object.prototype.toString will show its JavaScript wrapper object.
  value.getJSClass = Object.prototype.toString;
  this.jsclass = value.getJSClass();
  return this;
}
function TestValue( description  ) {
  this.description = description;
  this.type =  E_TYPE;
  this.jsclass = E_JSCLASS;
  return this;
}

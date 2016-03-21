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

gTestfile = 'wrapUnwrap.js';

/**
   File Name:          wrapUnwrap.js
   Section:            LiveConnect
   Description:

   Tests wrapping and unwrapping objects.
   @author mikeang

*/
var SECTION = "wrapUnwrap.js";
var VERSION = "JS1_3";
var TITLE   = "LiveConnect";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

var hashtable = new java.util.Hashtable();
var sameHashtable = hashtable;

jsEquals(hashtable,sameHashtable);
javaEquals(hashtable,sameHashtable);

function returnString(theString) {
  return theString;
}
var someString = new java.lang.String("foo");
var sameString = returnString(someString);
jsEquals(someString,sameString);
javaEquals(someString,sameString);

var assignToProperty = new Object();
assignToProperty.assignedString = someString;
jsEquals(someString,assignToProperty.assignedString);
javaEquals(someString,assignToProperty.assignedString);

function laConstructor(a,b,c) {
  this.one = a;
  this.two = b;
  this.three = c;
}
var stack1 = new java.util.Stack();
var stack2 = new java.util.Stack();
var num = 28;
var constructed = new laConstructor(stack1, stack2, num);
javaEquals(stack1, constructed.one);
javaEquals(stack2, constructed.two);
jsEquals(num, constructed.three);

test();

function jsEquals(expectedResult, actualResult, message) {
  new TestCase( SECTION,
		expectedResult +" == "+actualResult,
		expectedResult,
		actualResult );
}

function javaEquals(expectedResult, actualResult, message) {
  new TestCase( SECTION,
		expectedResult +" == "+actualResult,
		expectedResult,
		actualResult );
}

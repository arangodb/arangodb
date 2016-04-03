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

gTestfile = '15.9.5.js';

/**
   File Name:          15.9.5.js
   ECMA Section:       15.9.5 Properties of the Date prototype object
   Description:

   The Date prototype object is itself a Date object (its [[Class]] is
   "Date") whose value is NaN.

   The value of the internal [[Prototype]] property of the Date prototype
   object is the Object prototype object (15.2.3.1).

   In following descriptions of functions that are properties of the Date
   prototype object, the phrase "this Date object" refers to the object that
   is the this value for the invocation of the function; it is an error if
   this does not refer to an object for which the value of the internal
   [[Class]] property is "Date". Also, the phrase "this time value" refers
   to the number value for the time represented by this Date object, that is,
   the value of the internal [[Value]] property of this Date object.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Properties of the Date Prototype Object";

writeHeaderToLog( SECTION + " "+ TITLE);


Date.prototype.getClass = Object.prototype.toString;

new TestCase( SECTION,
	      "Date.prototype.getClass",
	      "[object Date]",
	      Date.prototype.getClass() );
new TestCase( SECTION,
	      "Date.prototype.valueOf()",
	      NaN,
	      Date.prototype.valueOf() );
test();


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

gTestfile = '15.4.5.1-2.js';

/**
   File Name:          15.4.5.1-2.js
   ECMA Section:       [[ Put]] (P, V)
   Description:
   Array objects use a variation of the [[Put]] method used for other native
   ECMAScript objects (section 8.6.2.2).

   Assume A is an Array object and P is a string.

   When the [[Put]] method of A is called with property P and value V, the
   following steps are taken:

   1.  Call the [[CanPut]] method of A with name P.
   2.  If Result(1) is false, return.
   3.  If A doesn't have a property with name P, go to step 7.
   4.  If P is "length", go to step 12.
   5.  Set the value of property P of A to V.
   6.  Go to step 8.
   7.  Create a property with name P, set its value to V and give it empty
   attributes.
   8.  If P is not an array index, return.
   9.  If A itself has a property (not an inherited property) named "length",
   andToUint32(P) is less than the value of the length property of A, then
   return.
   10. Change (or set) the value of the length property of A to ToUint32(P)+1.
   11. Return.
   12. Compute ToUint32(V).
   13. For every integer k that is less than the value of the length property
   of A but not less than Result(12), if A itself has a property (not an
   inherited property) named ToString(k), then delete that property.
   14. Set the value of property P of A to Result(12).
   15. Return.


   These are gTestcases from Waldemar, detailed in
   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=123552

   Author:             christine@netscape.com
   Date:               15 June 1998
*/

var SECTION = "15.4.5.1-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Array [[Put]] (P,V)";

writeHeaderToLog( SECTION + " "+ TITLE);

var a = new Array();

AddCase( "3.00", "three" );
AddCase( "00010", "eight" );
AddCase( "37xyz", "thirty-five" );
AddCase("5000000000", 5)
  AddCase( "-2", -3 );

new TestCase( SECTION,
	      "a[10]",
	      void 0,
	      a[10] );

new TestCase( SECTION,
	      "a[3]",
	      void 0,
	      a[3] );

a[4] = "four";

new TestCase( SECTION,
	      "a[4] = \"four\"; a[4]",
	      "four",
	      a[4] );

new TestCase( SECTION,
	      "a[\"4\"]",
	      "four",
	      a["4"] );

new TestCase( SECTION,
	      "a[\"4.00\"]",
	      void 0,
	      a["4.00"] );

new TestCase( SECTION,
	      "a.length",
	      5,
	      a.length );


a["5000000000"] = 5;

new TestCase( SECTION,
	      "a[\"5000000000\"] = 5; a.length",
	      5,
	      a.length );

new TestCase( SECTION,
	      "a[\"-2\"] = -3; a.length",
	      5,
	      a.length );

test();

function AddCase ( arg, value ) {

  a[arg] = value;

  new TestCase( SECTION,
		"a[\"" + arg + "\"] =  "+ value +"; a.length",
		0,
		a.length );
}

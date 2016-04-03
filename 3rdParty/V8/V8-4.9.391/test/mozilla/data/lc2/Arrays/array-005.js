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

gTestfile = 'array-005.js';

/**
   File Name:      array-005.js
   Description:

   Put and Get JavaArray Elements

   @author     christine@netscape.com
   @version    1.00
*/
var SECTION = "LiveConnect";
var VERSION = "1_3";
var TITLE   = "Java Array to JavaScript JavaArray object";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

//  In all test cases, the expected type is "object, and the expected
//  class is "JavaArray"

var E_TYPE = "object";
var E_CLASS = "[object JavaArray]";

var byte_array = ( new java.lang.String("hi") ).getBytes();

new TestCase(
  SECTION,
  "byte_array = new java.lang.String(\"hi\")).getBytes(); delete byte_array.length",
  false,
  delete byte_array.length );

new TestCase(
  SECTION,
  "byte_array[0]",
  ("hi").charCodeAt(0),
  byte_array[0]);

new TestCase(
  SECTION,
  "byte_array[1]",
  ("hi").charCodeAt(1),
  byte_array[1]);

byte_array.length = 0;

new TestCase(
  SECTION,
  "byte_array.length = 0; byte_array.length",
  2,
  byte_array.length );

var properties = "";
for ( var p in byte_array ) {
  properties += ( p == "length" ) ? p : "";
}

new TestCase(
  SECTION,
  "for ( var p in byte_array ) { properties += p ==\"length\" ? p : \"\" }; properties",
  "",
  properties );

new TestCase(
  SECTION,
  "byte_array[\"length\"]",
  2,
  byte_array["length"] );

byte_array["0"] = 127;

new TestCase(
  SECTION,
  "byte_array[\"0\"] = 127; byte_array[0]",
  127,
  byte_array[0] );

byte_array[1] = 99;

new TestCase(
  SECTION,
  "byte_array[1] = 99; byte_array[\"1\"]",
  99,
  byte_array["1"] );

test();


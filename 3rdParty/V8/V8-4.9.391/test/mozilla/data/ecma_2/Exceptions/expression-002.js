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
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communication Corporation.
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

gTestfile = 'expression-002.js';

/**
   File Name:          expressions-002.js
   Corresponds to:     ecma/Expressions/11.2.1-3-n.js
   ECMA Section:       11.2.1 Property Accessors
   Description:

   Try to access properties of an object whose value is undefined.

   Author:             christine@netscape.com
   Date:               09 september 1998
*/
var SECTION = "expressions-002.js";
var VERSION = "JS1_4";
var TITLE   = "Property Accessors";
writeHeaderToLog( SECTION + " "+TITLE );

startTest();

// go through all Native Function objects, methods, and properties and get their typeof.

var PROPERTY = new Array();
var p = 0;

// try to access properties of primitive types

OBJECT = new Property(  "undefined",    void 0,   "undefined",   NaN );

var result = "Failed";
var exception = "No exception thrown";
var expect = "Passed";

try {
  result = OBJECT.value.valueOf();
} catch ( e ) {
  result = expect;
  exception = e.toString();
}


new TestCase(
  SECTION,
  "Get the value of an object whose value is undefined "+
  "(threw " + exception +")",
  expect,
  result );

test();

function Property( object, value, string, number ) {
  this.object = object;
  this.string = String(value);
  this.number = Number(value);
  this.valueOf = value;
}

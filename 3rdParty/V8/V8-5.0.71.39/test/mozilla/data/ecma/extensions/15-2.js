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

gTestfile = '15-2.js';

/**
   File Name:          15-2.js
   ECMA Section:       15 Native ECMAScript Objects

   Description:        Every built-in function and every built-in constructor
   has the Function prototype object, which is the value of
   the expression Function.prototype as the value of its
   internal [[Prototype]] property, except the Function
   prototype object itself.

   That is, the __proto__ property of builtin functions and
   constructors should be the Function.prototype object.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Native ECMAScript Objects";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,  "Object.__proto__",   Function.prototype,   Object.__proto__ );
new TestCase( SECTION,  "Array.__proto__",    Function.prototype,   Array.__proto__ );
new TestCase( SECTION,  "String.__proto__",   Function.prototype,   String.__proto__ );
new TestCase( SECTION,  "Boolean.__proto__",  Function.prototype,   Boolean.__proto__ );
new TestCase( SECTION,  "Number.__proto__",   Function.prototype,   Number.__proto__ );
new TestCase( SECTION,  "Date.__proto__",     Function.prototype,   Date.__proto__ );
new TestCase( SECTION,  "TestCase.__proto__", Function.prototype,   TestCase.__proto__ );

new TestCase( SECTION,  "eval.__proto__",     Function.prototype,   eval.__proto__ );
new TestCase( SECTION,  "Math.pow.__proto__", Function.prototype,   Math.pow.__proto__ );
new TestCase( SECTION,  "String.prototype.indexOf.__proto__", Function.prototype,   String.prototype.indexOf.__proto__ );

test();

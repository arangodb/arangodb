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

gTestfile = '15.4.1.js';

/**
   File Name:          15.4.1.js
   ECMA Section:       15.4.1 The Array Constructor Called as a Function

   Description:        When Array is called as a function rather than as a
   constructor, it creates and initializes a new array
   object.  Thus, the function call Array(...) is
   equivalent to the object creationi new Array(...) with
   the same arguments.

   Author:             christine@netscape.com
   Date:               7 october 1997
*/

var SECTION = "15.4.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Array Constructor Called as a Function";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(   SECTION,
		"Array() +''",
		"",
		Array() +"" );

new TestCase(   SECTION,
		"typeof Array()",
		"object",
		typeof Array() );

new TestCase(   SECTION,
		"var arr = Array(); arr.getClass = Object.prototype.toString; arr.getClass()",
		"[object Array]",
		eval("var arr = Array(); arr.getClass = Object.prototype.toString; arr.getClass()") );

new TestCase(   SECTION,
		"var arr = Array(); arr.toString == Array.prototype.toString",
		true,
		eval("var arr = Array(); arr.toString == Array.prototype.toString") );

new TestCase(   SECTION,
		"Array().length",
		0,
		Array().length );

new TestCase(   SECTION,
		"Array(1,2,3) +''",
		"1,2,3",
		Array(1,2,3) +"" );

new TestCase(   SECTION,
		"typeof Array(1,2,3)",
		"object",
		typeof Array(1,2,3) );

new TestCase(   SECTION,
		"var arr = Array(1,2,3); arr.getClass = Object.prototype.toString; arr.getClass()",
		"[object Array]",
		eval("var arr = Array(1,2,3); arr.getClass = Object.prototype.toString; arr.getClass()") );

new TestCase(   SECTION,
		"var arr = Array(1,2,3); arr.toString == Array.prototype.toString",
		true,
		eval("var arr = Array(1,2,3); arr.toString == Array.prototype.toString") );

new TestCase(   SECTION,
		"Array(1,2,3).length",
		3,
		Array(1,2,3).length );

new TestCase(   SECTION,
		"typeof Array(12345)",
		"object",
		typeof Array(12345) );

new TestCase(   SECTION,
		"var arr = Array(12345); arr.getClass = Object.prototype.toString; arr.getClass()",
		"[object Array]",
		eval("var arr = Array(12345); arr.getClass = Object.prototype.toString; arr.getClass()") );

new TestCase(   SECTION,
		"var arr = Array(1,2,3,4,5); arr.toString == Array.prototype.toString",
		true,
		eval("var arr = Array(1,2,3,4,5); arr.toString == Array.prototype.toString") );

new TestCase(   SECTION,
		"Array(12345).length",
		12345,
		Array(12345).length );

test();

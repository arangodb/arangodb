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

gTestfile = '15.7.4.2-1.js';

/**
   File Name:          15.7.4.2.js
   ECMA Section:       15.7.4.2.1 Number.prototype.toString()
   Description:
   If the radix is the number 10 or not supplied, then this number value is
   given as an argument to the ToString operator; the resulting string value
   is returned.

   If the radix is supplied and is an integer from 2 to 36, but not 10, the
   result is a string, the choice of which is implementation dependent.

   The toString function is not generic; it generates a runtime error if its
   this value is not a Number object. Therefore it cannot be transferred to
   other kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               16 september 1997
*/
var SECTION = "15.7.4.2-1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Number.prototype.toString()");

//  the following two lines cause navigator to crash -- cmb 9/16/97
new TestCase(SECTION,
	     "Number.prototype.toString()",      
	     "0",       
	     eval("Number.prototype.toString()") );

new TestCase(SECTION,
	     "typeof(Number.prototype.toString())",
	     "string",     
	     eval("typeof(Number.prototype.toString())") );

new TestCase(SECTION, 
	     "s = Number.prototype.toString; o = new Number(); o.toString = s; o.toString()", 
	     "0",         
	     eval("s = Number.prototype.toString; o = new Number(); o.toString = s; o.toString()") );

new TestCase(SECTION, 
	     "s = Number.prototype.toString; o = new Number(1); o.toString = s; o.toString()",
	     "1",         
	     eval("s = Number.prototype.toString; o = new Number(1); o.toString = s; o.toString()") );

new TestCase(SECTION, 
	     "s = Number.prototype.toString; o = new Number(-1); o.toString = s; o.toString()",
	     "-1",        
	     eval("s = Number.prototype.toString; o = new Number(-1); o.toString = s; o.toString()") );

new TestCase(SECTION,
	     "var MYNUM = new Number(255); MYNUM.toString(10)",         
	     "255",     
	     eval("var MYNUM = new Number(255); MYNUM.toString(10)") );

new TestCase(SECTION,
	     "var MYNUM = new Number(Number.NaN); MYNUM.toString(10)",  
	     "NaN",     
	     eval("var MYNUM = new Number(Number.NaN); MYNUM.toString(10)") );

new TestCase(SECTION,
	     "var MYNUM = new Number(Infinity); MYNUM.toString(10)",  
	     "Infinity",  
	     eval("var MYNUM = new Number(Infinity); MYNUM.toString(10)") );

new TestCase(SECTION,
	     "var MYNUM = new Number(-Infinity); MYNUM.toString(10)",  
	     "-Infinity",
	     eval("var MYNUM = new Number(-Infinity); MYNUM.toString(10)") );

test();

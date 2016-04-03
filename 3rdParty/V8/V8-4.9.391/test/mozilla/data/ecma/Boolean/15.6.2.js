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

gTestfile = '15.6.2.js';

/**
   File Name:          15.6.2.js
   ECMA Section:       15.6.2 The Boolean Constructor
   15.6.2.1 new Boolean( value )
   15.6.2.2 new Boolean()

   This test verifies that the Boolean constructor
   initializes a new object (typeof should return
   "object").  The prototype of the new object should
   be Boolean.prototype.  The value of the object
   should be ToBoolean( value ) (a boolean value).

   Description:
   Author:             christine@netscape.com
   Date:               june 27, 1997

*/
var SECTION = "15.6.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "15.6.2 The Boolean Constructor; 15.6.2.1 new Boolean( value ); 15.6.2.2 new Boolean()";

writeHeaderToLog( SECTION + " "+ TITLE);

var array = new Array();
var item = 0;

new TestCase( SECTION,   "typeof (new Boolean(1))",         "object",            typeof (new Boolean(1)) );
new TestCase( SECTION,   "(new Boolean(1)).constructor",    Boolean.prototype.constructor,   (new Boolean(1)).constructor );
new TestCase( SECTION,
	      "TESTBOOL=new Boolean(1);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(1);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( SECTION,   "(new Boolean(1)).valueOf()",   true,       (new Boolean(1)).valueOf() );
new TestCase( SECTION,   "typeof new Boolean(1)",         "object",   typeof new Boolean(1) );
new TestCase( SECTION,   "(new Boolean(0)).constructor",    Boolean.prototype.constructor,   (new Boolean(0)).constructor );
new TestCase( SECTION,
	      "TESTBOOL=new Boolean(0);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(0);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( SECTION,   "(new Boolean(0)).valueOf()",   false,       (new Boolean(0)).valueOf() );
new TestCase( SECTION,   "typeof new Boolean(0)",         "object",   typeof new Boolean(0) );
new TestCase( SECTION,   "(new Boolean(-1)).constructor",    Boolean.prototype.constructor,   (new Boolean(-1)).constructor );
new TestCase( SECTION,
	      "TESTBOOL=new Boolean(-1);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(-1);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( SECTION,   "(new Boolean(-1)).valueOf()",   true,       (new Boolean(-1)).valueOf() );
new TestCase( SECTION,   "typeof new Boolean(-1)",         "object",   typeof new Boolean(-1) );
new TestCase( SECTION,   "(new Boolean('1')).constructor",    Boolean.prototype.constructor,   (new Boolean('1')).constructor );
new TestCase( SECTION,
	      "TESTBOOL=new Boolean('1');TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean('1');TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( SECTION,   "(new Boolean('1')).valueOf()",   true,       (new Boolean('1')).valueOf() );
new TestCase( SECTION,   "typeof new Boolean('1')",         "object",   typeof new Boolean('1') );
new TestCase( SECTION,   "(new Boolean('0')).constructor",    Boolean.prototype.constructor,   (new Boolean('0')).constructor );
new TestCase( SECTION,
	      "TESTBOOL=new Boolean('0');TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean('0');TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( SECTION,   "(new Boolean('0')).valueOf()",   true,       (new Boolean('0')).valueOf() );
new TestCase( SECTION,   "typeof new Boolean('0')",         "object",   typeof new Boolean('0') );
new TestCase( SECTION,   "(new Boolean('-1')).constructor",    Boolean.prototype.constructor,   (new Boolean('-1')).constructor );
new TestCase( SECTION,
	      "TESTBOOL=new Boolean('-1');TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean('-1');TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( SECTION,   "(new Boolean('-1')).valueOf()",   true,       (new Boolean('-1')).valueOf() );
new TestCase( SECTION,   "typeof new Boolean('-1')",         "object",   typeof new Boolean('-1') );
new TestCase( SECTION,   "(new Boolean(new Boolean(true))).constructor",    Boolean.prototype.constructor,   (new Boolean(new Boolean(true))).constructor );
new TestCase( SECTION,
	      "TESTBOOL=new Boolean(new Boolean(true));TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(new Boolean(true));TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( SECTION,   "(new Boolean(new Boolean(true))).valueOf()",   true,       (new Boolean(new Boolean(true))).valueOf() );
new TestCase( SECTION,   "typeof new Boolean(new Boolean(true))",         "object",   typeof new Boolean(new Boolean(true)) );
new TestCase( SECTION,   "(new Boolean(Number.NaN)).constructor",    Boolean.prototype.constructor,   (new Boolean(Number.NaN)).constructor );
new TestCase( SECTION,
	      "TESTBOOL=new Boolean(Number.NaN);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(Number.NaN);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( SECTION,   "(new Boolean(Number.NaN)).valueOf()",   false,       (new Boolean(Number.NaN)).valueOf() );
new TestCase( SECTION,   "typeof new Boolean(Number.NaN)",         "object",   typeof new Boolean(Number.NaN) );
new TestCase( SECTION,   "(new Boolean(null)).constructor",    Boolean.prototype.constructor,   (new Boolean(null)).constructor );
new TestCase( SECTION,
	      "TESTBOOL=new Boolean(null);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(null);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( SECTION,   "(new Boolean(null)).valueOf()",   false,       (new Boolean(null)).valueOf() );
new TestCase( SECTION,   "typeof new Boolean(null)",         "object",   typeof new Boolean(null) );
new TestCase( SECTION,   "(new Boolean(void 0)).constructor",    Boolean.prototype.constructor,   (new Boolean(void 0)).constructor );
new TestCase( SECTION,
	      "TESTBOOL=new Boolean(void 0);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(void 0);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( SECTION,   "(new Boolean(void 0)).valueOf()",   false,       (new Boolean(void 0)).valueOf() );
new TestCase( SECTION,   "typeof new Boolean(void 0)",         "object",   typeof new Boolean(void 0) );
new TestCase( SECTION,   "(new Boolean(Number.POSITIVE_INFINITY)).constructor",    Boolean.prototype.constructor,   (new Boolean(Number.POSITIVE_INFINITY)).constructor );
new TestCase( SECTION,
	      "TESTBOOL=new Boolean(Number.POSITIVE_INFINITY);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(Number.POSITIVE_INFINITY);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( SECTION,   "(new Boolean(Number.POSITIVE_INFINITY)).valueOf()",   true,       (new Boolean(Number.POSITIVE_INFINITY)).valueOf() );
new TestCase( SECTION,   "typeof new Boolean(Number.POSITIVE_INFINITY)",         "object",   typeof new Boolean(Number.POSITIVE_INFINITY) );
new TestCase( SECTION,   "(new Boolean(Number.NEGATIVE_INFINITY)).constructor",    Boolean.prototype.constructor,   (new Boolean(Number.NEGATIVE_INFINITY)).constructor );
new TestCase( SECTION,
	      "TESTBOOL=new Boolean(Number.NEGATIVE_INFINITY);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(Number.NEGATIVE_INFINITY);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( SECTION,   "(new Boolean(Number.NEGATIVE_INFINITY)).valueOf()",   true,       (new Boolean(Number.NEGATIVE_INFINITY)).valueOf() );
new TestCase( SECTION,   "typeof new Boolean(Number.NEGATIVE_INFINITY)",         "object",   typeof new Boolean(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION,   "(new Boolean(Number.NEGATIVE_INFINITY)).constructor",    Boolean.prototype.constructor,   (new Boolean(Number.NEGATIVE_INFINITY)).constructor );
new TestCase( "15.6.2.2",
	      "TESTBOOL=new Boolean();TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean();TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "15.6.2.2",   "(new Boolean()).valueOf()",   false,       (new Boolean()).valueOf() );
new TestCase( "15.6.2.2",   "typeof new Boolean()",        "object",    typeof new Boolean() );

test();

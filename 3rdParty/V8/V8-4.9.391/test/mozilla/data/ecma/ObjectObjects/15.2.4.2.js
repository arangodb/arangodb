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

gTestfile = '15.2.4.2.js';

/**
   File Name:          15.2.4.2.js
   ECMA Section:       15.2.4.2 Object.prototype.toString()

   Description:        When the toString method is called, the following
   steps are taken:
   1.  Get the [[Class]] property of this object
   2.  Call ToString( Result(1) )
   3.  Compute a string value by concatenating the three
   strings "[object " + Result(2) + "]"
   4.  Return Result(3).

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.2.4.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Object.prototype.toString()";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,  "(new Object()).toString()",    "[object Object]",  (new Object()).toString() );

new TestCase( SECTION,  "myvar = this;  myvar.toString = Object.prototype.toString; myvar.toString()",
	      GLOBAL.replace(/ @ 0x[0-9a-fA-F]+ \(native @ 0x[0-9a-fA-F]+\)/, ''),
	      eval("myvar = this;  myvar.toString = Object.prototype.toString; myvar.toString()")
  );

new TestCase( SECTION,  "myvar = MyObject; myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Function]",
	      eval("myvar = MyObject; myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( SECTION,  "myvar = new MyObject( true ); myvar.toString = Object.prototype.toString; myvar.toString()",
	      '[object Object]',
	      eval("myvar = new MyObject( true ); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( SECTION,  "myvar = new Number(0); myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Number]",
	      eval("myvar = new Number(0); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( SECTION,  "myvar = new String(''); myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object String]",
	      eval("myvar = new String(''); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( SECTION,  "myvar = Math; myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Math]",
	      eval("myvar = Math; myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( SECTION,  "myvar = new Function(); myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Function]",
	      eval("myvar = new Function(); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( SECTION,  "myvar = new Array(); myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Array]",
	      eval("myvar = new Array(); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( SECTION,  "myvar = new Boolean(); myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Boolean]",
	      eval("myvar = new Boolean(); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( SECTION,  "myvar = new Date(); myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Date]",
	      eval("myvar = new Date(); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( SECTION,  "var MYVAR = new Object( this ); MYVAR.toString()",
	      GLOBAL.replace(/ @ 0x[0-9a-fA-F]+ \(native @ 0x[0-9a-fA-F]+\)/, ''),
	      eval("var MYVAR = new Object( this ); MYVAR.toString()")
  );

new TestCase( SECTION,  "var MYVAR = new Object(); MYVAR.toString()",
	      "[object Object]",
	      eval("var MYVAR = new Object(); MYVAR.toString()") );

new TestCase( SECTION,  "var MYVAR = new Object(void 0); MYVAR.toString()",
	      "[object Object]",
	      eval("var MYVAR = new Object(void 0); MYVAR.toString()") );

new TestCase( SECTION,  "var MYVAR = new Object(null); MYVAR.toString()",
	      "[object Object]",
	      eval("var MYVAR = new Object(null); MYVAR.toString()") );


function MyObject( value ) {
  this.value = new Function( "return this.value" );
  this.toString = new Function ( "return this.value+''");
}

test();

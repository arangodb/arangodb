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

gTestfile = '15.3.1.1-1.js';

/**
   File Name:          15.3.1.1.js
   ECMA Section:       15.3.1.1 The Function Constructor Called as a Function

   Description:
   When the Function function is called with some arguments p1, p2, . . . , pn, body
   (where n might be 0, that is, there are no "p" arguments, and where body might
   also not be provided), the following steps are taken:

   1.  Create and return a new Function object exactly if the function constructor had
   been called with the same arguments (15.3.2.1).

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.3.1.1-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Function Constructor Called as a Function";

writeHeaderToLog( SECTION + " "+ TITLE);

var MyObject = Function( "value", "this.value = value; this.valueOf =  Function( 'return this.value' ); this.toString =  Function( 'return String(this.value);' )" );


var myfunc = Function();
myfunc.toString = Object.prototype.toString;

//    not going to test toString here since it is implementation dependent.
//    new TestCase( SECTION,  "myfunc.toString()",     "function anonymous() { }",    myfunc.toString() );

myfunc.toString = Object.prototype.toString;
new TestCase(   SECTION,
		"myfunc = Function(); myfunc.toString = Object.prototype.toString; myfunc.toString()",
		"[object Function]",
		myfunc.toString() );

new TestCase( SECTION, 
	      "myfunc.length",                           
	      0,                     
	      myfunc.length );

new TestCase( SECTION, 
	      "myfunc.prototype.toString()",             
	      "[object Object]",     
	      myfunc.prototype.toString() );

new TestCase( SECTION, 
	      "myfunc.prototype.constructor",            
	      myfunc,                
	      myfunc.prototype.constructor );

new TestCase( SECTION, 
	      "myfunc.arguments",                        
	      null,                  
	      myfunc.arguments );

new TestCase( SECTION, 
	      "var OBJ = new MyObject(true); OBJ.valueOf()",   
	      true,            
	      eval("var OBJ = new MyObject(true); OBJ.valueOf()") );

new TestCase( SECTION, 
	      "OBJ.toString()",                          
	      "true",                
	      OBJ.toString() );

new TestCase( SECTION, 
	      "OBJ.toString = Object.prototype.toString; OBJ.toString()",
	      "[object Object]", 
	      eval("OBJ.toString = Object.prototype.toString; OBJ.toString()") );

new TestCase( SECTION, 
	      "MyObject.toString = Object.prototype.toString; MyObject.toString()",   
	      "[object Function]",  
	      eval("MyObject.toString = Object.prototype.toString; MyObject.toString()") );

new TestCase( SECTION, 
	      "MyObject.length",                             
	      1,     
	      MyObject.length );

new TestCase( SECTION, 
	      "MyObject.prototype.constructor",              
	      MyObject,  
	      MyObject.prototype.constructor );

new TestCase( SECTION, 
	      "MyObject.arguments",                          
	      null,  
	      MyObject.arguments );
   
test();



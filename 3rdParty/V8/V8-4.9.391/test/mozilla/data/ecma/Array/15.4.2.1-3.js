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

gTestfile = '15.4.2.1-3.js';

/**
   File Name:          15.4.2.1-3.js
   ECMA Section:       15.4.2.1 new Array( item0, item1, ... )
   Description:        This description only applies of the constructor is
   given two or more arguments.

   The [[Prototype]] property of the newly constructed
   object is set to the original Array prototype object,
   the one that is the initial value of Array.prototype
   (15.4.3.1).

   The [[Class]] property of the newly constructed object
   is set to "Array".

   The length property of the newly constructed object is
   set to the number of arguments.

   The 0 property of the newly constructed object is set
   to item0... in general, for as many arguments as there
   are, the k property of the newly constructed object is
   set to argument k, where the first argument is
   considered to be argument number 0.

   This test stresses the number of arguments presented to
   the Array constructor.  Should support up to Math.pow
   (2,32) arguments, since that is the maximum length of an
   ECMAScript array.

   ***Change TEST_LENGTH to Math.pow(2,32) when larger array
   lengths are supported.

   Author:             christine@netscape.com
   Date:               7 october 1997
*/
var SECTION = "15.4.2.1-3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Array Constructor:  new Array( item0, item1, ...)";

writeHeaderToLog( SECTION + " "+ TITLE);

var TEST_STRING = "new Array(";
var ARGUMENTS = ""
  var TEST_LENGTH = Math.pow(2,10); //Math.pow(2,32);

for ( var index = 0; index < TEST_LENGTH; index++ ) {
  ARGUMENTS += index;
  ARGUMENTS += (index == (TEST_LENGTH-1) ) ? "" : ",";
}

TEST_STRING += ARGUMENTS + ")";

TEST_ARRAY = eval( TEST_STRING );

for ( var item = 0; item < TEST_LENGTH; item++ ) {
  new TestCase( SECTION,
		"TEST_ARRAY["+item+"]",    
		item,   
		TEST_ARRAY[item] );
}

new TestCase( SECTION,
	      "new Array( ["+TEST_LENGTH+" arguments] ) +''", 
	      ARGUMENTS,         
	      TEST_ARRAY +"" );

new TestCase( SECTION,
	      "TEST_ARRAY.toString",                          
	      Array.prototype.toString,  
	      TEST_ARRAY.toString );

new TestCase( SECTION,
	      "TEST_ARRAY.join",                              
	      Array.prototype.join,      
	      TEST_ARRAY.join );

new TestCase( SECTION,
	      "TEST_ARRAY.sort",                              
	      Array.prototype.sort,      
	      TEST_ARRAY.sort );

new TestCase( SECTION,
	      "TEST_ARRAY.reverse",                           
	      Array.prototype.reverse,   
	      TEST_ARRAY.reverse );

new TestCase( SECTION,
	      "TEST_ARRAY.length",                            
	      TEST_LENGTH,       
	      TEST_ARRAY.length );

new TestCase( SECTION,
	      "TEST_ARRAY.toString = Object.prototype.toString; TEST_ARRAY.toString()",
	      "[object Array]",
	      eval("TEST_ARRAY.toString = Object.prototype.toString; TEST_ARRAY.toString()") );

test();

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

gTestfile = '15.4.1.1.js';

/**
   File Name:          15.4.1.1.js
   ECMA Section:       15.4.1 Array( item0, item1,... )

   Description:        When Array is called as a function rather than as a
   constructor, it creates and initializes a new array
   object.  Thus, the function call Array(...) is
   equivalent to the object creation new Array(...) with
   the same arguments.

   An array is created and returned as if by the expression
   new Array( item0, item1, ... ).

   Author:             christine@netscape.com
   Date:               7 october 1997
*/
var SECTION = "15.4.1.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Array Constructor Called as a Function";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "typeof Array(1,2)",       
	      "object",          
	      typeof Array(1,2) );

new TestCase( SECTION,
	      "(Array(1,2)).toString",   
	      Array.prototype.toString,   
	      (Array(1,2)).toString );

new TestCase( SECTION,
	      "var arr = Array(1,2,3); arr.toString = Object.prototype.toString; arr.toString()",
	      "[object Array]",
	      eval("var arr = Array(1,2,3); arr.toString = Object.prototype.toString; arr.toString()") );

new TestCase( SECTION,
	      "(Array(1,2)).length",     
	      2,                 
	      (Array(1,2)).length );

new TestCase( SECTION,
	      "var arr = (Array(1,2)); arr[0]", 
	      1,          
	      eval("var arr = (Array(1,2)); arr[0]") );

new TestCase( SECTION,
	      "var arr = (Array(1,2)); arr[1]", 
	      2,          
	      eval("var arr = (Array(1,2)); arr[1]") );

new TestCase( SECTION,
	      "var arr = (Array(1,2)); String(arr)", 
	      "1,2", 
	      eval("var arr = (Array(1,2)); String(arr)") );

test();

function ToUint32( n ) {
  n = Number( n );
  if( isNaN(n) || n == 0 || n == Number.POSITIVE_INFINITY ||
      n == Number.NEGATIVE_INFINITY ) {
    return 0;
  }
  var sign = n < 0 ? -1 : 1;

  return ( sign * ( n * Math.floor( Math.abs(n) ) ) ) % Math.pow(2, 32);
}


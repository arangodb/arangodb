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

gTestfile = '15.4.1.2.js';

/**
   File Name:          15.4.1.2.js
   ECMA Section:       15.4.1.2 Array(len)

   Description:        When Array is called as a function rather than as a
   constructor, it creates and initializes a new array
   object.  Thus, the function call Array(...) is
   equivalent to the object creationi new Array(...) with
   the same arguments.

   An array is created and returned as if by the
   expression new Array(len).

   Author:             christine@netscape.com
   Date:               7 october 1997
*/
var SECTION = "15.4.1.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Array Constructor Called as a Function:  Array(len)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, 
	      "(Array()).length",            
	      0,                             
	      (Array()).length );

new TestCase( SECTION,
	      "(Array(0)).length",           
	      0,                             
	      (Array(0)).length );

new TestCase( SECTION,
	      "(Array(1)).length",           
	      1,                             
	      (Array(1)).length );

new TestCase( SECTION,
	      "(Array(10)).length",          
	      10,                            
	      (Array(10)).length );

new TestCase( SECTION,
	      "(Array('1')).length",         
	      1,                             
	      (Array('1')).length );

new TestCase( SECTION,
	      "(Array(1000)).length",        
	      1000,                          
	      (Array(1000)).length );

new TestCase( SECTION,
	      "(Array('1000')).length",      
	      1,                             
	      (Array('1000')).length );

new TestCase( SECTION,
	      "(Array(4294967295)).length",  
	      ToUint32(4294967295),          
	      (Array(4294967295)).length );

new TestCase( SECTION,
	      "(Array(Math.pow(2,31)-1)).length",    
	      ToUint32(Math.pow(2,31)-1),    
	      (Array(Math.pow(2,31)-1)).length );

new TestCase( SECTION,
	      "(Array(Math.pow(2,31))).length",      
	      ToUint32(Math.pow(2,31)),      
	      (Array(Math.pow(2,31))).length );

new TestCase( SECTION,
	      "(Array(Math.pow(2,31)+1)).length",    
	      ToUint32(Math.pow(2,31)+1),    
	      (Array(Math.pow(2,31)+1)).length );

new TestCase( SECTION,
	      "(Array('8589934592')).length",
	      1,                             
	      (Array("8589934592")).length );

new TestCase( SECTION,
	      "(Array('4294967296')).length",
	      1,                             
	      (Array("4294967296")).length );

new TestCase( SECTION,
	      "(Array(1073741823)).length",  
	      ToUint32(1073741823),          
	      (Array(1073741823)).length );

new TestCase( SECTION,
	      "(Array(1073741824)).length",  
	      ToUint32(1073741824),	       
	      (Array(1073741824)).length );

new TestCase( SECTION,
	      "(Array('a string')).length",  
	      1,                             
	      (Array("a string")).length );

test();

function ToUint32( n ) {
  n = Number( n );
  var sign = ( n < 0 ) ? -1 : 1;

  if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY) {
    return 0;
  }
  n = sign * Math.floor( Math.abs(n) )

    n = n % Math.pow(2,32);

  if ( n < 0 ){
    n += Math.pow(2,32);
  }

  return ( n );
}

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

gTestfile = '15.4.4.2.js';

/**
   File Name:          15.4.4.2.js
   ECMA Section:       15.4.4.2 Array.prototype.toString()
   Description:        The elements of this object are converted to strings
   and these strings are then concatenated, separated by
   comma characters.  The result is the same as if the
   built-in join method were invoiked for this object
   with no argument.
   Author:             christine@netscape.com
   Date:               7 october 1997
*/

var SECTION = "15.4.4.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Array.prototype.toString";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, 
	      "Array.prototype.toString.length", 
	      0, 
	      Array.prototype.toString.length );

new TestCase( SECTION, 
	      "(new Array()).toString()",    
	      "",    
	      (new Array()).toString() );

new TestCase( SECTION, 
	      "(new Array(2)).toString()",   
	      ",",   
	      (new Array(2)).toString() );

new TestCase( SECTION, 
	      "(new Array(0,1)).toString()", 
	      "0,1", 
	      (new Array(0,1)).toString() );

new TestCase( SECTION, 
	      "(new Array( Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY)).toString()", 
	      "NaN,Infinity,-Infinity",  
	      (new Array( Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY)).toString() );

new TestCase( SECTION, 
	      "(new Array( Boolean(1), Boolean(0))).toString()",  
	      "true,false",  
	      (new Array(Boolean(1),Boolean(0))).toString() );

new TestCase( SECTION, 
	      "(new Array(void 0,null)).toString()",   
	      ",",   
	      (new Array(void 0,null)).toString() );

var EXPECT_STRING = "";
var MYARR = new Array();

for ( var i = -50; i < 50; i+= 0.25 ) {
  MYARR[MYARR.length] = i;
  EXPECT_STRING += i +",";
}

EXPECT_STRING = EXPECT_STRING.substring( 0, EXPECT_STRING.length -1 );

new TestCase( SECTION,
	      "MYARR.toString()", 
	      EXPECT_STRING, 
	      MYARR.toString() );

test();

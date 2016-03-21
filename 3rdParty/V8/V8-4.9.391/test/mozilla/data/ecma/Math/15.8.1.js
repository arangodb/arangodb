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

gTestfile = '15.8.1.js';

/**
   File Name:          15.8.1.js
   ECMA Section:       15.8.1.js   Value Properties of the Math Object
   15.8.1.1    E
   15.8.1.2    LN10
   15.8.1.3    LN2
   15.8.1.4    LOG2E
   15.8.1.5    LOG10E
   15.8.1.6    PI
   15.8.1.7    SQRT1_2
   15.8.1.8    SQRT2
   Description:        verify the values of some math constants
   Author:             christine@netscape.com
   Date:               7 july 1997

*/
var SECTION = "15.8.1"
  var VERSION = "ECMA_1";
startTest();
var TITLE   = "Value Properties of the Math Object";

writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase( "15.8.1.1", "Math.E",            
	      2.7182818284590452354, 
	      Math.E );

new TestCase( "15.8.1.1",
	      "typeof Math.E",     
	      "number",              
	      typeof Math.E );

new TestCase( "15.8.1.2",
	      "Math.LN10",         
	      2.302585092994046,     
	      Math.LN10 );

new TestCase( "15.8.1.2",
	      "typeof Math.LN10",  
	      "number",              
	      typeof Math.LN10 );

new TestCase( "15.8.1.3",
	      "Math.LN2",         
	      0.6931471805599453,    
	      Math.LN2 );

new TestCase( "15.8.1.3",
	      "typeof Math.LN2",   
	      "number",              
	      typeof Math.LN2 );

new TestCase( "15.8.1.4",
	      "Math.LOG2E",        
	      1.4426950408889634,    
	      Math.LOG2E );

new TestCase( "15.8.1.4",
	      "typeof Math.LOG2E", 
	      "number",              
	      typeof Math.LOG2E );

new TestCase( "15.8.1.5",
	      "Math.LOG10E",       
	      0.4342944819032518,    
	      Math.LOG10E);

new TestCase( "15.8.1.5",
	      "typeof Math.LOG10E",
	      "number",              
	      typeof Math.LOG10E);

new TestCase( "15.8.1.6",
	      "Math.PI",           
	      3.14159265358979323846,
	      Math.PI );

new TestCase( "15.8.1.6",
	      "typeof Math.PI",    
	      "number",              
	      typeof Math.PI );

new TestCase( "15.8.1.7",
	      "Math.SQRT1_2",      
	      0.7071067811865476,    
	      Math.SQRT1_2);

new TestCase( "15.8.1.7",
	      "typeof Math.SQRT1_2",
	      "number",             
	      typeof Math.SQRT1_2);

new TestCase( "15.8.1.8",
	      "Math.SQRT2",        
	      1.4142135623730951,    
	      Math.SQRT2 );

new TestCase( "15.8.1.8",
	      "typeof Math.SQRT2", 
	      "number",              
	      typeof Math.SQRT2 );

new TestCase( SECTION, 
	      "var MATHPROPS='';for( p in Math ){ MATHPROPS +=p; };MATHPROPS",
	      "",
	      eval("var MATHPROPS='';for( p in Math ){ MATHPROPS +=p; };MATHPROPS") );

test();

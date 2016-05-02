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

gTestfile = '15.8.2.1.js';

/**
   File Name:          15.8.2.1.js
   ECMA Section:       15.8.2.1 abs( x )
   Description:        return the absolute value of the argument,
   which should be the magnitude of the argument
   with a positive sign.
   -   if x is NaN, return NaN
   -   if x is -0, result is +0
   -   if x is -Infinity, result is +Infinity
   Author:             christine@netscape.com
   Date:               7 july 1997
*/
var SECTION = "15.8.2.1";
var VERSION = "ECMA_1";
var TITLE   = "Math.abs()";
var BUGNUMBER = "77391";
startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,  
	      "Math.abs.length",
	      1,
              Math.abs.length );

new TestCase( SECTION,
	      "Math.abs()", 
	      Number.NaN,
	      Math.abs() );

new TestCase( SECTION,
	      "Math.abs( void 0 )", 
	      Number.NaN,
	      Math.abs(void 0) );

new TestCase( SECTION,
	      "Math.abs( null )",  
	      0,   
	      Math.abs(null) );

new TestCase( SECTION,
	      "Math.abs( true )", 
	      1,    
	      Math.abs(true) );

new TestCase( SECTION,
	      "Math.abs( false )", 
	      0,     
	      Math.abs(false) );

new TestCase( SECTION,
	      "Math.abs( string primitive)",
	      Number.NaN, 
	      Math.abs("a string primitive") );

new TestCase( SECTION,  
	      "Math.abs( string object )",  
	      Number.NaN,    
	      Math.abs(new String( 'a String object' ))  );

new TestCase( SECTION,  
	      "Math.abs( Number.NaN )", 
	      Number.NaN,
	      Math.abs(Number.NaN) );

new TestCase( SECTION,
	      "Math.abs(0)", 
	      0,
              Math.abs( 0 ) );

new TestCase( SECTION, 
	      "Math.abs( -0 )", 
	      0,  
	      Math.abs(-0) );

new TestCase( SECTION,  
	      "Infinity/Math.abs(-0)",
	      Infinity, 
	      Infinity/Math.abs(-0) );

new TestCase( SECTION,  
	      "Math.abs( -Infinity )",      
	      Number.POSITIVE_INFINITY,  
	      Math.abs( Number.NEGATIVE_INFINITY ) );

new TestCase( SECTION,  
	      "Math.abs( Infinity )",  
	      Number.POSITIVE_INFINITY,
	      Math.abs( Number.POSITIVE_INFINITY ) );

new TestCase( SECTION,  
	      "Math.abs( - MAX_VALUE )",   
	      Number.MAX_VALUE,
	      Math.abs( - Number.MAX_VALUE )       );

new TestCase( SECTION,  
	      "Math.abs( - MIN_VALUE )",
	      Number.MIN_VALUE,
	      Math.abs( -Number.MIN_VALUE )        );

new TestCase( SECTION,  
	      "Math.abs( MAX_VALUE )",  
	      Number.MAX_VALUE,  
	      Math.abs( Number.MAX_VALUE )       );

new TestCase( SECTION, 
	      "Math.abs( MIN_VALUE )",
	      Number.MIN_VALUE, 
	      Math.abs( Number.MIN_VALUE )        );

new TestCase( SECTION,  
	      "Math.abs( -1 )",    
	      1,   
	      Math.abs( -1 )                       );

new TestCase( SECTION,  
	      "Math.abs( new Number( -1 ) )",
	      1,   
	      Math.abs( new Number(-1) )           );

new TestCase( SECTION,  
	      "Math.abs( 1 )",  
	      1, 
	      Math.abs( 1 ) );

new TestCase( SECTION,  
	      "Math.abs( Math.PI )", 
	      Math.PI,   
	      Math.abs( Math.PI ) );

new TestCase( SECTION,
	      "Math.abs( -Math.PI )", 
	      Math.PI,  
	      Math.abs( -Math.PI ) );

new TestCase( SECTION,
	      "Math.abs(-1/100000000)",
	      1/100000000,  
	      Math.abs(-1/100000000) );

new TestCase( SECTION,
	      "Math.abs(-Math.pow(2,32))", 
	      Math.pow(2,32),    
	      Math.abs(-Math.pow(2,32)) );

new TestCase( SECTION,  
	      "Math.abs(Math.pow(2,32))",
	      Math.pow(2,32), 
	      Math.abs(Math.pow(2,32)) );

new TestCase( SECTION,
	      "Math.abs( -0xfff )", 
	      4095,    
	      Math.abs( -0xfff ) );

new TestCase( SECTION,
	      "Math.abs( -0777 )", 
	      511,   
	      Math.abs(-0777 ) );

new TestCase( SECTION,
	      "Math.abs('-1e-1')",  
	      0.1,  
	      Math.abs('-1e-1') );

new TestCase( SECTION, 
	      "Math.abs('0xff')",  
	      255,  
	      Math.abs('0xff') );

new TestCase( SECTION,
	      "Math.abs('077')",   
	      77,   
	      Math.abs('077') );

new TestCase( SECTION, 
	      "Math.abs( 'Infinity' )",
	      Infinity,
	      Math.abs('Infinity') );

new TestCase( SECTION,
	      "Math.abs( '-Infinity' )",
	      Infinity,
	      Math.abs('-Infinity') );

test();

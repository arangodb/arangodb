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

gTestfile = '11.13.2-2.js';

/**
   File Name:          11.13.2-2js
   ECMA Section:       11.13.2 Compound Assignment: /=
   Description:

   *= /= %= += -= <<= >>= >>>= &= ^= |=

   11.13.2 Compound assignment ( op= )

   The production AssignmentExpression :
   LeftHandSideExpression @ = AssignmentExpression, where @ represents one of
   the operators indicated above, is evaluated as follows:

   1.  Evaluate LeftHandSideExpression.
   2.  Call GetValue(Result(1)).
   3.  Evaluate AssignmentExpression.
   4.  Call GetValue(Result(3)).
   5.  Apply operator @ to Result(2) and Result(4).
   6.  Call PutValue(Result(1), Result(5)).
   7.  Return Result(5).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.13.2-2";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Compound Assignment: /=");


// NaN cases

new TestCase( SECTION,   
              "VAR1 = NaN; VAR2=1; VAR1 /= VAR2",      
              Number.NaN,
              eval("VAR1 = Number.NaN; VAR2=1; VAR1 /= VAR2") );

new TestCase( SECTION,   
              "VAR1 = NaN; VAR2=1; VAR1 /= VAR2; VAR1",
              Number.NaN,
              eval("VAR1 = Number.NaN; VAR2=1; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = NaN; VAR2=0; VAR1 /= VAR2",      
              Number.NaN,
              eval("VAR1 = Number.NaN; VAR2=0; VAR1 /= VAR2") );

new TestCase( SECTION,   
              "VAR1 = NaN; VAR2=0; VAR1 /= VAR2; VAR1",
              Number.NaN,
              eval("VAR1 = Number.NaN; VAR2=0; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2=NaN; VAR1 /= VAR2",      
              Number.NaN,
              eval("VAR1 = 0; VAR2=Number.NaN; VAR1 /= VAR2") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2=NaN; VAR1 /= VAR2; VAR1",
              Number.NaN,
              eval("VAR1 = 0; VAR2=Number.NaN; VAR1 /= VAR2; VAR1") );

// number cases
new TestCase( SECTION,   
              "VAR1 = 0; VAR2=1; VAR1 /= VAR2",        
              0,         
              eval("VAR1 = 0; VAR2=1; VAR1 /= VAR2") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2=1; VAR1 /= VAR2;VAR1",   
              0,         
              eval("VAR1 = 0; VAR2=1; VAR1 /= VAR2;VAR1") );

new TestCase( SECTION,   
              "VAR1 = 0xFF; VAR2 = 0xA, VAR1 /= VAR2",
              25.5,     
              eval("VAR1 = 0XFF; VAR2 = 0XA, VAR1 /= VAR2") );

// special division cases

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= Infinity; VAR1 /= VAR2",   
              0,     
              eval("VAR1 = 0; VAR2 = Number.POSITIVE_INFINITY; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= Infinity; VAR1 /= VAR2",  
              0,     
              eval("VAR1 = -0; VAR2 = Number.POSITIVE_INFINITY; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= -Infinity; VAR1 /= VAR2", 
              0,     
              eval("VAR1 = -0; VAR2 = Number.NEGATIVE_INFINITY; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= -Infinity; VAR1 /= VAR2",  
              0,     
              eval("VAR1 = 0; VAR2 = Number.NEGATIVE_INFINITY; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= Infinity; VAR2 /= VAR1",   
              Number.POSITIVE_INFINITY,     
              eval("VAR1 = 0; VAR2 = Number.POSITIVE_INFINITY; VAR2 /= VAR1; VAR2") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= Infinity; VAR2 /= VAR1",  
              Number.NEGATIVE_INFINITY,     
              eval("VAR1 = -0; VAR2 = Number.POSITIVE_INFINITY; VAR2 /= VAR1; VAR2") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= -Infinity; VAR2 /= VAR1", 
              Number.POSITIVE_INFINITY,     
              eval("VAR1 = -0; VAR2 = Number.NEGATIVE_INFINITY; VAR2 /= VAR1; VAR2") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= -Infinity; VAR2 /= VAR1",  
              Number.NEGATIVE_INFINITY,     
              eval("VAR1 = 0; VAR2 = Number.NEGATIVE_INFINITY; VAR2 /= VAR1; VAR2") );

new TestCase( SECTION,   
              "VAR1 = Infinity; VAR2= Infinity; VAR1 /= VAR2",  
              Number.NaN,     
              eval("VAR1 = Number.POSITIVE_INFINITY; VAR2 = Number.POSITIVE_INFINITY; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = Infinity; VAR2= -Infinity; VAR1 /= VAR2", 
              Number.NaN,     
              eval("VAR1 = Number.POSITIVE_INFINITY; VAR2 = Number.NEGATIVE_INFINITY; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 =-Infinity; VAR2= Infinity; VAR1 /= VAR2",  
              Number.NaN,     
              eval("VAR1 = Number.NEGATIVE_INFINITY; VAR2 = Number.POSITIVE_INFINITY; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 =-Infinity; VAR2=-Infinity; VAR1 /= VAR2",  
              Number.NaN,     
              eval("VAR1 = Number.NEGATIVE_INFINITY; VAR2 = Number.NEGATIVE_INFINITY; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= 0; VAR1 /= VAR2",   
              Number.NaN,     
              eval("VAR1 = 0; VAR2 = 0; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= -0; VAR1 /= VAR2",  
              Number.NaN,    
              eval("VAR1 = 0; VAR2 = -0; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= 0; VAR1 /= VAR2",  
              Number.NaN,     
              eval("VAR1 = -0; VAR2 = 0; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= -0; VAR1 /= VAR2", 
              Number.NaN,     
              eval("VAR1 = -0; VAR2 = -0; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 1; VAR2= 0; VAR1 /= VAR2",   
              Number.POSITIVE_INFINITY,     
              eval("VAR1 = 1; VAR2 = 0; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 1; VAR2= -0; VAR1 /= VAR2",  
              Number.NEGATIVE_INFINITY,     
              eval("VAR1 = 1; VAR2 = -0; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -1; VAR2= 0; VAR1 /= VAR2",  
              Number.NEGATIVE_INFINITY,     
              eval("VAR1 = -1; VAR2 = 0; VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -1; VAR2= -0; VAR1 /= VAR2", 
              Number.POSITIVE_INFINITY,     
              eval("VAR1 = -1; VAR2 = -0; VAR1 /= VAR2; VAR1") );

// string cases
new TestCase( SECTION,   
              "VAR1 = 1000; VAR2 = '10', VAR1 /= VAR2; VAR1",
              100,      
              eval("VAR1 = 1000; VAR2 = '10', VAR1 /= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = '1000'; VAR2 = 10, VAR1 /= VAR2; VAR1",
              100,      
              eval("VAR1 = '1000'; VAR2 = 10, VAR1 /= VAR2; VAR1") );
/*
  new TestCase( SECTION,    "VAR1 = 10; VAR2 = '0XFF', VAR1 /= VAR2", 2550,       eval("VAR1 = 10; VAR2 = '0XFF', VAR1 /= VAR2") );
  new TestCase( SECTION,    "VAR1 = '0xFF'; VAR2 = 0xA, VAR1 /= VAR2", 2550,      eval("VAR1 = '0XFF'; VAR2 = 0XA, VAR1 /= VAR2") );

  new TestCase( SECTION,    "VAR1 = '10'; VAR2 = '255', VAR1 /= VAR2", 2550,      eval("VAR1 = '10'; VAR2 = '255', VAR1 /= VAR2") );
  new TestCase( SECTION,    "VAR1 = '10'; VAR2 = '0XFF', VAR1 /= VAR2", 2550,     eval("VAR1 = '10'; VAR2 = '0XFF', VAR1 /= VAR2") );
  new TestCase( SECTION,    "VAR1 = '0xFF'; VAR2 = 0xA, VAR1 /= VAR2", 2550,      eval("VAR1 = '0XFF'; VAR2 = 0XA, VAR1 /= VAR2") );

  // boolean cases
  new TestCase( SECTION,    "VAR1 = true; VAR2 = false; VAR1 /= VAR2",    0,      eval("VAR1 = true; VAR2 = false; VAR1 /= VAR2") );
  new TestCase( SECTION,    "VAR1 = true; VAR2 = true; VAR1 /= VAR2",    1,      eval("VAR1 = true; VAR2 = true; VAR1 /= VAR2") );

  // object cases
  new TestCase( SECTION,    "VAR1 = new Boolean(true); VAR2 = 10; VAR1 /= VAR2;VAR1",    10,      eval("VAR1 = new Boolean(true); VAR2 = 10; VAR1 /= VAR2; VAR1") );
  new TestCase( SECTION,    "VAR1 = new Number(11); VAR2 = 10; VAR1 /= VAR2; VAR1",    110,      eval("VAR1 = new Number(11); VAR2 = 10; VAR1 /= VAR2; VAR1") );
  new TestCase( SECTION,    "VAR1 = new Number(11); VAR2 = new Number(10); VAR1 /= VAR2",    110,      eval("VAR1 = new Number(11); VAR2 = new Number(10); VAR1 /= VAR2") );
  new TestCase( SECTION,    "VAR1 = new String('15'); VAR2 = new String('0xF'); VAR1 /= VAR2",    255,      eval("VAR1 = String('15'); VAR2 = new String('0xF'); VAR1 /= VAR2") );

*/

test();


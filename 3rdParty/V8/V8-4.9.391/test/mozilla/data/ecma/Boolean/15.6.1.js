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

gTestfile = '15.6.1.js';

/**
   File Name:      15.6.1.js
   ECMA Section:   15.6.1 The Boolean Function
   15.6.1.1 Boolean( value )
   15.6.1.2 Boolean ()
   Description:    Boolean( value ) should return a Boolean value
   not a Boolean object) computed by
   Boolean.toBooleanValue( value)

   15.6.1.2 Boolean() returns false

   Author:         christine@netscape.com
   Date:           27 jun 1997


   Data File Fields:
   VALUE       Argument passed to the Boolean function
   TYPE        typeof VALUE (not used, but helpful in understanding
   the data file)
   E_RETURN    Expected return value of Boolean( VALUE )
*/
var SECTION = "15.6.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Boolean constructor called as a function: Boolean( value ) and Boolean()";

writeHeaderToLog( SECTION + " "+ TITLE);

var array = new Array();
var item = 0;

new TestCase( SECTION,   "Boolean(1)",         true,   Boolean(1) );
new TestCase( SECTION,   "Boolean(0)",         false,  Boolean(0) );
new TestCase( SECTION,   "Boolean(-1)",        true,   Boolean(-1) );
new TestCase( SECTION,   "Boolean('1')",       true,   Boolean("1") );
new TestCase( SECTION,   "Boolean('0')",       true,   Boolean("0") );
new TestCase( SECTION,   "Boolean('-1')",      true,   Boolean("-1") );
new TestCase( SECTION,   "Boolean(true)",      true,   Boolean(true) );
new TestCase( SECTION,   "Boolean(false)",     false,  Boolean(false) );

new TestCase( SECTION,   "Boolean('true')",    true,   Boolean("true") );
new TestCase( SECTION,   "Boolean('false')",   true,   Boolean("false") );
new TestCase( SECTION,   "Boolean(null)",      false,  Boolean(null) );

new TestCase( SECTION,   "Boolean(-Infinity)", true,   Boolean(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION,   "Boolean(NaN)",       false,  Boolean(Number.NaN) );
new TestCase( SECTION,   "Boolean(void(0))",   false,  Boolean( void(0) ) );
new TestCase( SECTION,   "Boolean(x=0)",       false,  Boolean( x=0 ) );
new TestCase( SECTION,   "Boolean(x=1)",       true,   Boolean( x=1 ) );
new TestCase( SECTION,   "Boolean(x=false)",   false,  Boolean( x=false ) );
new TestCase( SECTION,   "Boolean(x=true)",    true,   Boolean( x=true ) );
new TestCase( SECTION,   "Boolean(x=null)",    false,  Boolean( x=null ) );
new TestCase( SECTION,   "Boolean()",          false,  Boolean() );
//    array[item++] = new TestCase( SECTION,   "Boolean(var someVar)",     false,  Boolean( someVar ) );

test();

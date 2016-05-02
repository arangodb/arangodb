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

gTestfile = '15.1.2.6.js';

/**
   File Name:          15.1.2.6.js
   ECMA Section:       15.1.2.6 isNaN( x )

   Description:        Applies ToNumber to its argument, then returns true if
   the result isNaN and otherwise returns false.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.1.2.6";
var VERSION = "ECMA_1";
var TITLE   = "isNaN( x )";
var BUGNUMBER = "none";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, "isNaN.length",      1,                  isNaN.length );
new TestCase( SECTION, "var MYPROPS=''; for ( var p in isNaN ) { MYPROPS+= p }; MYPROPS", "prototype", eval("var MYPROPS=''; for ( var p in isNaN ) { MYPROPS+= p }; MYPROPS") );
new TestCase( SECTION, "isNaN.length = null; isNaN.length", 1,      eval("isNaN.length=null; isNaN.length") );
new TestCase( SECTION, "delete isNaN.length",               false,  delete isNaN.length );
new TestCase( SECTION, "delete isNaN.length; isNaN.length", 1,      eval("delete isNaN.length; isNaN.length") );

//     new TestCase( SECTION, "isNaN.__proto__",   Function.prototype, isNaN.__proto__ );

new TestCase( SECTION, "isNaN()",           true,               isNaN() );
new TestCase( SECTION, "isNaN( null )",     false,              isNaN(null) );
new TestCase( SECTION, "isNaN( void 0 )",   true,               isNaN(void 0) );
new TestCase( SECTION, "isNaN( true )",     false,              isNaN(true) );
new TestCase( SECTION, "isNaN( false)",     false,              isNaN(false) );
new TestCase( SECTION, "isNaN( ' ' )",      false,              isNaN( " " ) );

new TestCase( SECTION, "isNaN( 0 )",        false,              isNaN(0) );
new TestCase( SECTION, "isNaN( 1 )",        false,              isNaN(1) );
new TestCase( SECTION, "isNaN( 2 )",        false,              isNaN(2) );
new TestCase( SECTION, "isNaN( 3 )",        false,              isNaN(3) );
new TestCase( SECTION, "isNaN( 4 )",        false,              isNaN(4) );
new TestCase( SECTION, "isNaN( 5 )",        false,              isNaN(5) );
new TestCase( SECTION, "isNaN( 6 )",        false,              isNaN(6) );
new TestCase( SECTION, "isNaN( 7 )",        false,              isNaN(7) );
new TestCase( SECTION, "isNaN( 8 )",        false,              isNaN(8) );
new TestCase( SECTION, "isNaN( 9 )",        false,              isNaN(9) );

new TestCase( SECTION, "isNaN( '0' )",        false,              isNaN('0') );
new TestCase( SECTION, "isNaN( '1' )",        false,              isNaN('1') );
new TestCase( SECTION, "isNaN( '2' )",        false,              isNaN('2') );
new TestCase( SECTION, "isNaN( '3' )",        false,              isNaN('3') );
new TestCase( SECTION, "isNaN( '4' )",        false,              isNaN('4') );
new TestCase( SECTION, "isNaN( '5' )",        false,              isNaN('5') );
new TestCase( SECTION, "isNaN( '6' )",        false,              isNaN('6') );
new TestCase( SECTION, "isNaN( '7' )",        false,              isNaN('7') );
new TestCase( SECTION, "isNaN( '8' )",        false,              isNaN('8') );
new TestCase( SECTION, "isNaN( '9' )",        false,              isNaN('9') );


new TestCase( SECTION, "isNaN( 0x0a )",    false,              isNaN( 0x0a ) );
new TestCase( SECTION, "isNaN( 0xaa )",    false,              isNaN( 0xaa ) );
new TestCase( SECTION, "isNaN( 0x0A )",    false,              isNaN( 0x0A ) );
new TestCase( SECTION, "isNaN( 0xAA )",    false,              isNaN( 0xAA ) );

new TestCase( SECTION, "isNaN( '0x0a' )",    false,              isNaN( "0x0a" ) );
new TestCase( SECTION, "isNaN( '0xaa' )",    false,              isNaN( "0xaa" ) );
new TestCase( SECTION, "isNaN( '0x0A' )",    false,              isNaN( "0x0A" ) );
new TestCase( SECTION, "isNaN( '0xAA' )",    false,              isNaN( "0xAA" ) );

new TestCase( SECTION, "isNaN( 077 )",      false,              isNaN( 077 ) );
new TestCase( SECTION, "isNaN( '077' )",    false,              isNaN( "077" ) );


new TestCase( SECTION, "isNaN( Number.NaN )",   true,              isNaN(Number.NaN) );
new TestCase( SECTION, "isNaN( Number.POSITIVE_INFINITY )", false,  isNaN(Number.POSITIVE_INFINITY) );
new TestCase( SECTION, "isNaN( Number.NEGATIVE_INFINITY )", false,  isNaN(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION, "isNaN( Number.MAX_VALUE )",         false,  isNaN(Number.MAX_VALUE) );
new TestCase( SECTION, "isNaN( Number.MIN_VALUE )",         false,  isNaN(Number.MIN_VALUE) );

new TestCase( SECTION, "isNaN( NaN )",               true,      isNaN(NaN) );
new TestCase( SECTION, "isNaN( Infinity )",          false,     isNaN(Infinity) );

new TestCase( SECTION, "isNaN( 'Infinity' )",               false,  isNaN("Infinity") );
new TestCase( SECTION, "isNaN( '-Infinity' )",              false,  isNaN("-Infinity") );

test();

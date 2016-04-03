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

gTestfile = '15.1.2.7.js';

/**
   File Name:          15.1.2.7.js
   ECMA Section:       15.1.2.7 isFinite(number)

   Description:        Applies ToNumber to its argument, then returns false if
   the result is NaN, Infinity, or -Infinity, and otherwise
   returns true.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.1.2.7";
var VERSION = "ECMA_1";
var TITLE   = "isFinite( x )";
var BUGNUMBER= "none";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, "isFinite.length",      1,                  isFinite.length );
new TestCase( SECTION, "isFinite.length = null; isFinite.length",   1,      eval("isFinite.length=null; isFinite.length") );
new TestCase( SECTION, "delete isFinite.length",                    false,  delete isFinite.length );
new TestCase( SECTION, "delete isFinite.length; isFinite.length",   1,      eval("delete isFinite.length; isFinite.length") );
new TestCase( SECTION, "var MYPROPS=''; for ( p in isFinite ) { MYPROPS+= p }; MYPROPS",    "prototype", eval("var MYPROPS=''; for ( p in isFinite ) { MYPROPS += p }; MYPROPS") );

new TestCase( SECTION,  "isFinite()",           false,              isFinite() );
new TestCase( SECTION, "isFinite( null )",      true,              isFinite(null) );
new TestCase( SECTION, "isFinite( void 0 )",    false,             isFinite(void 0) );
new TestCase( SECTION, "isFinite( false )",     true,              isFinite(false) );
new TestCase( SECTION, "isFinite( true)",       true,              isFinite(true) );
new TestCase( SECTION, "isFinite( ' ' )",       true,              isFinite( " " ) );

new TestCase( SECTION, "isFinite( new Boolean(true) )",     true,   isFinite(new Boolean(true)) );
new TestCase( SECTION, "isFinite( new Boolean(false) )",    true,   isFinite(new Boolean(false)) );

new TestCase( SECTION, "isFinite( 0 )",        true,              isFinite(0) );
new TestCase( SECTION, "isFinite( 1 )",        true,              isFinite(1) );
new TestCase( SECTION, "isFinite( 2 )",        true,              isFinite(2) );
new TestCase( SECTION, "isFinite( 3 )",        true,              isFinite(3) );
new TestCase( SECTION, "isFinite( 4 )",        true,              isFinite(4) );
new TestCase( SECTION, "isFinite( 5 )",        true,              isFinite(5) );
new TestCase( SECTION, "isFinite( 6 )",        true,              isFinite(6) );
new TestCase( SECTION, "isFinite( 7 )",        true,              isFinite(7) );
new TestCase( SECTION, "isFinite( 8 )",        true,              isFinite(8) );
new TestCase( SECTION, "isFinite( 9 )",        true,              isFinite(9) );

new TestCase( SECTION, "isFinite( '0' )",        true,              isFinite('0') );
new TestCase( SECTION, "isFinite( '1' )",        true,              isFinite('1') );
new TestCase( SECTION, "isFinite( '2' )",        true,              isFinite('2') );
new TestCase( SECTION, "isFinite( '3' )",        true,              isFinite('3') );
new TestCase( SECTION, "isFinite( '4' )",        true,              isFinite('4') );
new TestCase( SECTION, "isFinite( '5' )",        true,              isFinite('5') );
new TestCase( SECTION, "isFinite( '6' )",        true,              isFinite('6') );
new TestCase( SECTION, "isFinite( '7' )",        true,              isFinite('7') );
new TestCase( SECTION, "isFinite( '8' )",        true,              isFinite('8') );
new TestCase( SECTION, "isFinite( '9' )",        true,              isFinite('9') );

new TestCase( SECTION, "isFinite( 0x0a )",    true,                 isFinite( 0x0a ) );
new TestCase( SECTION, "isFinite( 0xaa )",    true,                 isFinite( 0xaa ) );
new TestCase( SECTION, "isFinite( 0x0A )",    true,                 isFinite( 0x0A ) );
new TestCase( SECTION, "isFinite( 0xAA )",    true,                 isFinite( 0xAA ) );

new TestCase( SECTION, "isFinite( '0x0a' )",    true,               isFinite( "0x0a" ) );
new TestCase( SECTION, "isFinite( '0xaa' )",    true,               isFinite( "0xaa" ) );
new TestCase( SECTION, "isFinite( '0x0A' )",    true,               isFinite( "0x0A" ) );
new TestCase( SECTION, "isFinite( '0xAA' )",    true,               isFinite( "0xAA" ) );

new TestCase( SECTION, "isFinite( 077 )",       true,               isFinite( 077 ) );
new TestCase( SECTION, "isFinite( '077' )",     true,               isFinite( "077" ) );

new TestCase( SECTION, "isFinite( new String('Infinity') )",        false,      isFinite(new String("Infinity")) );
new TestCase( SECTION, "isFinite( new String('-Infinity') )",       false,      isFinite(new String("-Infinity")) );

new TestCase( SECTION, "isFinite( 'Infinity' )",        false,      isFinite("Infinity") );
new TestCase( SECTION, "isFinite( '-Infinity' )",       false,      isFinite("-Infinity") );
new TestCase( SECTION, "isFinite( Number.POSITIVE_INFINITY )",  false,  isFinite(Number.POSITIVE_INFINITY) );
new TestCase( SECTION, "isFinite( Number.NEGATIVE_INFINITY )",  false,  isFinite(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION, "isFinite( Number.NaN )",                false,  isFinite(Number.NaN) );

new TestCase( SECTION, "isFinite( Infinity )",  false,  isFinite(Infinity) );
new TestCase( SECTION, "isFinite( -Infinity )",  false,  isFinite(-Infinity) );
new TestCase( SECTION, "isFinite( NaN )",                false,  isFinite(NaN) );


new TestCase( SECTION, "isFinite( Number.MAX_VALUE )",          true,  isFinite(Number.MAX_VALUE) );
new TestCase( SECTION, "isFinite( Number.MIN_VALUE )",          true,  isFinite(Number.MIN_VALUE) );

test();

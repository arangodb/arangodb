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

gTestfile = '11.8.1.js';

/**
   File Name:          11.8.1.js
   ECMA Section:       11.8.1  The less-than operator ( < )
   Description:


   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.8.1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " The less-than operator ( < )");

new TestCase( SECTION, "true < false",              false,      true < false );
new TestCase( SECTION, "false < true",              true,       false < true );
new TestCase( SECTION, "false < false",             false,      false < false );
new TestCase( SECTION, "true < true",               false,      true < true );

new TestCase( SECTION, "new Boolean(true) < new Boolean(true)",     false,  new Boolean(true) < new Boolean(true) );
new TestCase( SECTION, "new Boolean(true) < new Boolean(false)",    false,  new Boolean(true) < new Boolean(false) );
new TestCase( SECTION, "new Boolean(false) < new Boolean(true)",    true,   new Boolean(false) < new Boolean(true) );
new TestCase( SECTION, "new Boolean(false) < new Boolean(false)",   false,  new Boolean(false) < new Boolean(false) );

new TestCase( SECTION, "new MyObject(Infinity) < new MyObject(Infinity)",   false,  new MyObject( Number.POSITIVE_INFINITY ) < new MyObject( Number.POSITIVE_INFINITY) );
new TestCase( SECTION, "new MyObject(-Infinity) < new MyObject(Infinity)",  true,   new MyObject( Number.NEGATIVE_INFINITY ) < new MyObject( Number.POSITIVE_INFINITY) );
new TestCase( SECTION, "new MyObject(-Infinity) < new MyObject(-Infinity)", false,  new MyObject( Number.NEGATIVE_INFINITY ) < new MyObject( Number.NEGATIVE_INFINITY) );

new TestCase( SECTION, "new MyValueObject(false) < new MyValueObject(true)",  true,   new MyValueObject(false) < new MyValueObject(true) );
new TestCase( SECTION, "new MyValueObject(true) < new MyValueObject(true)",   false,  new MyValueObject(true) < new MyValueObject(true) );
new TestCase( SECTION, "new MyValueObject(false) < new MyValueObject(false)", false,  new MyValueObject(false) < new MyValueObject(false) );

new TestCase( SECTION, "new MyStringObject(false) < new MyStringObject(true)",  true,   new MyStringObject(false) < new MyStringObject(true) );
new TestCase( SECTION, "new MyStringObject(true) < new MyStringObject(true)",   false,  new MyStringObject(true) < new MyStringObject(true) );
new TestCase( SECTION, "new MyStringObject(false) < new MyStringObject(false)", false,  new MyStringObject(false) < new MyStringObject(false) );

new TestCase( SECTION, "Number.NaN < Number.NaN",   false,     Number.NaN < Number.NaN );
new TestCase( SECTION, "0 < Number.NaN",            false,     0 < Number.NaN );
new TestCase( SECTION, "Number.NaN < 0",            false,     Number.NaN < 0 );

new TestCase( SECTION, "0 < -0",                    false,      0 < -0 );
new TestCase( SECTION, "-0 < 0",                    false,      -0 < 0 );

new TestCase( SECTION, "Infinity < 0",                  false,      Number.POSITIVE_INFINITY < 0 );
new TestCase( SECTION, "Infinity < Number.MAX_VALUE",   false,      Number.POSITIVE_INFINITY < Number.MAX_VALUE );
new TestCase( SECTION, "Infinity < Infinity",           false,      Number.POSITIVE_INFINITY < Number.POSITIVE_INFINITY );

new TestCase( SECTION, "0 < Infinity",                  true,       0 < Number.POSITIVE_INFINITY );
new TestCase( SECTION, "Number.MAX_VALUE < Infinity",   true,       Number.MAX_VALUE < Number.POSITIVE_INFINITY );

new TestCase( SECTION, "0 < -Infinity",                 false,      0 < Number.NEGATIVE_INFINITY );
new TestCase( SECTION, "Number.MAX_VALUE < -Infinity",  false,      Number.MAX_VALUE < Number.NEGATIVE_INFINITY );
new TestCase( SECTION, "-Infinity < -Infinity",         false,      Number.NEGATIVE_INFINITY < Number.NEGATIVE_INFINITY );

new TestCase( SECTION, "-Infinity < 0",                 true,       Number.NEGATIVE_INFINITY < 0 );
new TestCase( SECTION, "-Infinity < -Number.MAX_VALUE", true,       Number.NEGATIVE_INFINITY < -Number.MAX_VALUE );
new TestCase( SECTION, "-Infinity < Number.MIN_VALUE",  true,       Number.NEGATIVE_INFINITY < Number.MIN_VALUE );

new TestCase( SECTION, "'string' < 'string'",           false,       'string' < 'string' );
new TestCase( SECTION, "'astring' < 'string'",          true,       'astring' < 'string' );
new TestCase( SECTION, "'strings' < 'stringy'",         true,       'strings' < 'stringy' );
new TestCase( SECTION, "'strings' < 'stringier'",       false,       'strings' < 'stringier' );
new TestCase( SECTION, "'string' < 'astring'",          false,      'string' < 'astring' );
new TestCase( SECTION, "'string' < 'strings'",          true,       'string' < 'strings' );

test();

function MyObject(value) {
  this.value = value;
  this.valueOf = new Function( "return this.value" );
  this.toString = new Function( "return this.value +''" );
}
function MyValueObject(value) {
  this.value = value;
  this.valueOf = new Function( "return this.value" );
}
function MyStringObject(value) {
  this.value = value;
  this.toString = new Function( "return this.value +''" );
}

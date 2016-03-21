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

gTestfile = 'constructor.js';

/**
 * Tests constructing some Java classes from JS.
 * Tests constructor matching algorithm.
 *
 * this needs to be converted to the new test structure
 *
 * @author cbegle and mikeang
 */

var SECTION = "wrapUnwrap.js";
var VERSION = "JS1_3";
var TITLE   = "LiveConnect";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

var char_object = java.lang.Character.forDigit(22, 36);
test_typeof( "string", char_object+"a" );

var boolean_object = new java.lang.Boolean( true );
test_class( "java.lang.Boolean", boolean_object );

var boolean_object = new java.lang.Boolean( false );
test_class( "java.lang.Boolean", boolean_object );

var integer_object = new java.lang.Integer( 12345 );
test_class( "java.lang.Integer", integer_object );

var string_object = new java.lang.String( "string object value" );
test_class( "java.lang.String", string_object );

// This doesn't work - bug #105857
var float_object = new java.lang.Float( .009 * .009 );
test_class( "java.lang.Float", float_object );

var double_object = new java.lang.Double( java.lang.Math.PI );
test_class( "java.lang.Double", double_object );

var long_object = new java.lang.Long( 92233720368547760 );
test_class( "java.lang.Long", long_object );

var rect_object = new java.awt.Rectangle( 0, 0, 100, 100 );
test_class ( "java.awt.Rectangle", rect_object );

test();


function test_typeof( eType, someObject ) {
  new TestCase( SECTION,
		"typeof( " +someObject+")",
		eType,
		typeof someObject );
}

/**
 * Implements Java instanceof keyword.
 */
function test_class( eClass, javaObject ) {
  new TestCase( SECTION,
		javaObject +".getClass().equals( java.lang.Class.forName( " +
		eClass +")",
		true,
		(javaObject.getClass()).equals( java.lang.Class.forName(eClass)) );
}

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

gTestfile = 'tostring-1.js';

/**
   File Name:          tostring-1.js
   Section:            Function.toString
   Description:

   Since the behavior of Function.toString() is implementation-dependent,
   toString tests for function are not in the ECMA suite.

   Currently, an attempt to parse the toString output for some functions
   and verify that the result is something reasonable.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "tostring-1";
var VERSION = "JS1_2";
startTest();
var TITLE   = "Function.toString()";

writeHeaderToLog( SECTION + " "+ TITLE);

var tab = "    ";

t1 = new TestFunction( "stub", "value", tab + "return value;" );

t2 = new TestFunction( "ToString", "object", tab+"return object + \"\";" );

t3 = new TestFunction( "Add", "a, b, c, d, e",  tab +"var s = a + b + c + d + e;\n" +
		       tab + "return s;" );

t4 = new TestFunction( "noop", "value" );

t5 = new TestFunction( "anonymous", "", tab+"return \"hello!\";" );

var f = new Function( "return \"hello!\"");

new TestCase( SECTION,
	      "stub.toString()",
	      t1.valueOf(),
	      stub.toString() );

new TestCase( SECTION,
	      "ToString.toString()",
	      t2.valueOf(),
	      ToString.toString() );

new TestCase( SECTION,
	      "Add.toString()",
	      t3.valueOf(),
	      Add.toString() );

new TestCase( SECTION,
	      "noop.toString()",
	      t4.toString(),
	      noop.toString() );

new TestCase( SECTION,
	      "f.toString()",
	      t5.toString(),
	      f.toString() );
test();

function noop( value ) {
}
function Add( a, b, c, d, e ) {
  var s = a + b + c + d + e;
  return s;
}
function stub( value ) {
  return value;
}
function ToString( object ) {
  return object + "";
}

function ToBoolean( value ) {
  if ( value == 0 || value == NaN || value == false ) {
    return false;
  } else {
    return true;
  }
}

function TestFunction( name, args, body ) {
  if ( name == "anonymous" && version() == 120 ) {
    name = "";
  }

  this.name = name;
  this.arguments = args.toString();
  this.body = body;

  /* the format of Function.toString() in JavaScript 1.2 is:
     function name ( arguments ) {
     body
     }
  */
  this.value = "function " + (name ? name : "" )+
    "("+args+") {\n"+ (( body ) ? body +"\n" : "") + "}";

  this.toString = new Function( "return this.value" );
  this.valueOf = new Function( "return this.value" );
  return this;
}

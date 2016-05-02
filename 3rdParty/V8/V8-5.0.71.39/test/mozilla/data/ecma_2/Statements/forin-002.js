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
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communication Corporation.
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

gTestfile = 'forin-002.js';

/**
 *  File Name:          forin-002.js
 *  ECMA Section:
 *  Description:        The forin-001 statement
 *
 *  Verify that the property name is assigned to the property on the left
 *  hand side of the for...in expression.
 *
 *  Author:             christine@netscape.com
 *  Date:               28 August 1998
 */
var SECTION = "forin-002";
var VERSION = "ECMA_2";
var TITLE   = "The for...in  statement";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

function MyObject( value ) {
  this.value = value;
  this.valueOf = new Function ( "return this.value" );
  this.toString = new Function ( "return this.value + \"\"" );
  this.toNumber = new Function ( "return this.value + 0" );
  this.toBoolean = new Function ( "return Boolean( this.value )" );
}

ForIn_1(this);
ForIn_2(this);

ForIn_1(new MyObject(true));
ForIn_2(new MyObject(new Boolean(true)));

ForIn_2(3);

test();

/**
 *  For ... In in a With Block
 *
 */
function ForIn_1( object) {
  with ( object ) {
    for ( property in object ) {
      new TestCase(
	SECTION,
	"with loop in a for...in loop.  ("+object+")["+property +"] == "+
	"eval ( " + property +" )",
	true,
	object[property] == eval(property) );
    }
  }
}

/**
 *  With block in a For...In loop
 *
 */
function ForIn_2(object) {
  for ( property in object ) {
    with ( object ) {
      new TestCase(
	SECTION,
	"with loop in a for...in loop.  ("+object+")["+property +"] == "+
	"eval ( " + property +" )",
	true,
	object[property] == eval(property) );
    }
  }
}


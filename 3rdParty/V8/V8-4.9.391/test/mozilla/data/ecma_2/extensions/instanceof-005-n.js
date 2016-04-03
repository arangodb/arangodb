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

gTestfile = 'instanceof-005-n.js';

/**
 *  File Name:          instanceof-001.js
 *  ECMA Section:       11.8.6
 *  Description:
 *
 *  RelationalExpression instanceof Identifier
 *
 *  Author:             christine@netscape.com
 *  Date:               2 September 1998
 */
var SECTION = "instanceof-005-n";
var VERSION = "ECMA_2";
var TITLE   = "instanceof"

  startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

function InstanceOf( object_1, object_2, expect ) {
  result = object_1 instanceof object_2;

  new TestCase(
    SECTION,
    "(" + object_1 + ") instanceof " + object_2,
    expect,
    result );
}

function Gen3(value) {
  this.value = value;
  this.generation = 3;
  this.toString = new Function ( "return \"(Gen\"+this.generation+\" instance)\"" );
}
Gen3.name = 3;
Gen3.__proto__.toString = new Function( "return \"(\"+this.name+\" object)\"");

function Gen2(value) {
  this.value = value;
  this.generation = 2;
}
Gen2.name = 2;
Gen2.prototype = new Gen3();

function Gen1(value) {
  this.value = value;
  this.generation = 1;
}
Gen1.name = 1;
Gen1.prototype = new Gen2();

function Gen0(value) {
  this.value = value;
  this.generation = 0;
}
Gen0.name = 0;
Gen0.prototype = new Gen1();


function GenA(value) {
  this.value = value;
  this.generation = "A";
  this.toString = new Function ( "return \"(instance of Gen\"+this.generation+\")\"" );

}
GenA.prototype = new Gen0();
GenA.name = "A";

function GenB(value) {
  this.value = value;
  this.generation = "B";
  this.toString = new Function ( "return \"(instance of Gen\"+this.generation+\")\"" );
}
GenB.name = "B"
  GenB.prototype = void 0;


// Identifier is a function, prototype of Identifier is not an object

DESCRIPTION = "Identifier is a function, prototype of Identifier is not an object";
EXPECTED = "error";

InstanceOf( new GenB(), GenB, "error" );

test();

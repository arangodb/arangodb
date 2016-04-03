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

gTestfile = '11.4.1.js';

/**
   File Name:          11.4.1.js
   ECMA Section:       11.4.1 the Delete Operator
   Description:        returns true if the property could be deleted
   returns false if it could not be deleted
   Author:             christine@netscape.com
   Date:               7 july 1997

*/


var SECTION = "11.4.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The delete operator";

writeHeaderToLog( SECTION + " "+ TITLE);

//    new TestCase( SECTION,   "x=[9,8,7];delete(x[2]);x.length",         2,             eval("x=[9,8,7];delete(x[2]);x.length") );
//    new TestCase( SECTION,   "x=[9,8,7];delete(x[2]);x.toString()",     "9,8",         eval("x=[9,8,7];delete(x[2]);x.toString()") );
new TestCase( SECTION,   "x=new Date();delete x;typeof(x)",        "undefined",    eval("x=new Date();delete x;typeof(x)") );

//    array[item++] = new TestCase( SECTION,   "delete(x=new Date())",        true,   delete(x=new Date()) );
//    array[item++] = new TestCase( SECTION,   "delete('string primitive')",   true,   delete("string primitive") );
//    array[item++] = new TestCase( SECTION,   "delete(new String( 'string object' ) )",  true,   delete(new String("string object")) );
//    array[item++] = new TestCase( SECTION,   "delete(new Number(12345) )",  true,   delete(new Number(12345)) );
new TestCase( SECTION,   "delete(Math.PI)",             false,   delete(Math.PI) );
//    array[item++] = new TestCase( SECTION,   "delete(null)",                true,   delete(null) );
//    array[item++] = new TestCase( SECTION,   "delete(void(0))",             true,   delete(void(0)) );

// variables declared with the var statement are not deletable.

var abc;
new TestCase( SECTION,   "var abc; delete(abc)",        false,   delete abc );

new TestCase(   SECTION,
                "var OB = new MyObject(); for ( p in OB ) { delete p }",
                true,
                eval("var OB = new MyObject(); for ( p in OB ) { delete p }") );

test();

function MyObject() {
  this.prop1 = true;
  this.prop2 = false;
  this.prop3 = null;
  this.prop4 = void 0;
  this.prop5 = "hi";
  this.prop6 = 42;
  this.prop7 = new Date();
  this.prop8 = Math.PI;
}

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

gTestfile = '11.2.1-2.js';

/**
   File Name:          11.2.1-2.js
   ECMA Section:       11.2.1 Property Accessors
   Description:

   Properties are accessed by name, using either the dot notation:
   MemberExpression . Identifier
   CallExpression . Identifier

   or the bracket notation:    MemberExpression [ Expression ]
   CallExpression [ Expression ]

   The dot notation is explained by the following syntactic conversion:
   MemberExpression . Identifier
   is identical in its behavior to
   MemberExpression [ <identifier-string> ]
   and similarly
   CallExpression . Identifier
   is identical in its behavior to
   CallExpression [ <identifier-string> ]
   where <identifier-string> is a string literal containing the same sequence
   of characters as the Identifier.

   The production MemberExpression : MemberExpression [ Expression ] is
   evaluated as follows:

   1.  Evaluate MemberExpression.
   2.  Call GetValue(Result(1)).
   3.  Evaluate Expression.
   4.  Call GetValue(Result(3)).
   5.  Call ToObject(Result(2)).
   6.  Call ToString(Result(4)).
   7.  Return a value of type Reference whose base object is Result(5) and
   whose property name is Result(6).

   The production CallExpression : CallExpression [ Expression ] is evaluated
   in exactly the same manner, except that the contained CallExpression is
   evaluated in step 1.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.2.1-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Property Accessors";
writeHeaderToLog( SECTION + " "+TITLE );

// go through all Native Function objects, methods, and properties and get their typeof.

var PROPERTY = new Array();
var p = 0;

// try to access properties of primitive types

PROPERTY[p++] = new Property(  "\"hi\"",    "hi",   "hi",   NaN );
PROPERTY[p++] = new Property(  NaN,         NaN,    "NaN",    NaN );
//    PROPERTY[p++] = new Property(  3,           3,      "3",    3  );
PROPERTY[p++] = new Property(  true,        true,      "true",    1 );
PROPERTY[p++] = new Property(  false,       false,      "false",    0 );

for ( var i = 0, RESULT; i < PROPERTY.length; i++ ) {
  new TestCase( SECTION,
                PROPERTY[i].object + ".valueOf()",
                PROPERTY[i].value,
                eval( PROPERTY[i].object+ ".valueOf()" ) );

  new TestCase( SECTION,
                PROPERTY[i].object + ".toString()",
                PROPERTY[i].string,
                eval( PROPERTY[i].object+ ".toString()" ) );

}

test();

function MyObject( value ) {
  this.value = value;
  this.stringValue = value +"";
  this.numberValue = Number(value);
  return this;
}
function Property( object, value, string, number ) {
  this.object = object;
  this.string = String(value);
  this.number = Number(value);
  this.value = value;
}

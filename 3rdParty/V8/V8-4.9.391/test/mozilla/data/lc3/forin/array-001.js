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

gTestfile = 'array-001.js';

/**
 *  Verify that for-in loops can be used with java objects.
 *
 *  Java array members should be enumerated in for... in loops.
 *
 *
 */
var SECTION = "array-001";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0:  for ... in java objects";
SECTION;
startTest();

// we just need to know the names of all the expected enumerated
// properties.  we will get the values to the original objects.

// for arrays, we just need to know the length, since java arrays
// don't have any extra properties


var dt = new Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass;

var a = new Array();

a[a.length] = new TestObject(
  new java.lang.String("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789").getBytes(),
  "new java.lang.String(\"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\").getBytes()",
  36 );

a[a.length] = new TestObject(
  dt.PUB_ARRAY_SHORT,
  "dt.PUB_ARRAY_SHORT",
  dt.PUB_ARRAY_SHORT.length );

a[a.length] = new TestObject(
  dt.PUB_ARRAY_LONG,
  "dt.PUB_ARRAY_LONG",
  dt.PUB_ARRAY_LONG.length );

a[a.length] = new TestObject(
  dt.PUB_ARRAY_DOUBLE,
  "dt.PUB_ARRAY_DOUBLE",
  dt.PUB_ARRAY_DOUBLE.length );

a[a.length] = new TestObject(
  dt.PUB_ARRAY_BYTE,
  "dt.PUB_ARRAY_BYTE",
  dt.PUB_ARRAY_BYTE.length );

a[a.length] = new TestObject(
  dt.PUB_ARRAY_CHAR,
  "dt.PUB_ARRAY_CHAR",
  dt.PUB_ARRAY_CHAR.length );

a[a.length] = new TestObject(
  dt.PUB_ARRAY_OBJECT,
  "dt.PUB_ARRAY_OBJECT",
  dt.PUB_ARRAY_OBJECT.length );

for ( var i = 0; i < a.length; i++ ) {
  // check the number of properties of the enumerated object
  new TestCase(
    a[i].description +"; length",
    a[i].items,
    a[i].enumedArray.pCount );

  for ( var arrayItem = 0; arrayItem < a[i].items; arrayItem++ ) {
    new TestCase(
      "["+arrayItem+"]",
      a[i].javaArray[arrayItem],
      a[i].enumedArray[arrayItem] );
  }
}

test();

function TestObject( arr, descr, len ) {
  this.javaArray = arr;
  this.description = descr;
  this.items    = len;
  this.enumedArray = new enumObject(arr);
}

function enumObject( o ) {
  this.pCount = 0;
  for ( var p in o ) {
    this.pCount++;
    if ( !isNaN(p) ) {
      eval( "this["+p+"] = o["+p+"]" );
    } else {
      eval( "this." + p + " = o["+ p+"]" );
    }
  }
}


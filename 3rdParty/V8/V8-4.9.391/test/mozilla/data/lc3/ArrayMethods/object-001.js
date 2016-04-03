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

gTestfile = 'object-001.js';

/**
 *  java array objects "inherit" JS string methods.  verify that byte arrays
 *  can inherit JavaScript Array object methods join, reverse, sort and valueOf
 *
 */
var SECTION = "java array object inheritance JavaScript Array methods";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 " + SECTION;

startTest();

dt = new Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass();

obArray = dt.PUB_ARRAY_OBJECT;

// check string value

new TestCase(
  "dt = new Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass(); "+
  "obArray = dt.PUB_ARRAY_OBJECT" +
  "obArray.join() +''",
  join(obArray),
  obArray.join() );

// check type of object returned by method

new TestCase(
  "typeof obArray.reverse().join()",
  reverse(obArray),
  obArray.reverse().join() );

new TestCase(
  "obArray.reverse().getClass().getName() +''",
  "[Ljava.lang.Object;",
  obArray.reverse().getClass().getName() +'');

test();

function join( a ) {
  for ( var i = 0, s = ""; i < a.length; i++ ) {
    s += a[i].toString() + ( i + 1 < a.length ? "," : "" );
  }
  return s;
}
function reverse( a ) {
  for ( var i = a.length -1, s = ""; i >= 0; i-- ) {
    s += a[i].toString() + ( i> 0 ? "," : "" );
  }
  return s;
}

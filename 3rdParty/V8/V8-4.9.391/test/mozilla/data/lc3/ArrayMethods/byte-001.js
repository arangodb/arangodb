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

gTestfile = 'byte-001.js';

/**
 *  java array objects "inherit" JS string methods.  verify that byte arrays
 *  can inherit JavaScript Array object methods
 *
 *
 */
var SECTION = "java array object inheritance JavaScript Array methods";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 " + SECTION;

startTest();

var a = new Array();

a[a.length] = new TestObject(
  "var b"+a.length+" = new java.lang.String(\"hello\").getBytes(); b"+a.length+".join() +''",
  "b"+a.length,
  "join",
  true,
  "104,101,108,108,111" );

a[a.length] = new TestObject(
  "var b"+a.length+" = new java.lang.String(\"JavaScript\").getBytes(); b"+a.length+".reverse().join() +''",
  "b"+a.length,
  "reverse",
  true,
  getCharValues("tpircSavaJ") );

a[a.length] = new TestObject(
  "var b"+a.length+" = new java.lang.String(\"JavaScript\").getBytes(); b"+a.length+".sort().join() +''",
  "b"+a.length,
  "sort",
  true,
  "105,112,114,116,118,74,83,97,97,99" );

a[a.length] = new TestObject(
  "var b"+a.length+" = new java.lang.String(\"JavaScript\").getBytes(); b"+a.length+".sort().join() +''",
  "b"+a.length,
  "sort",
  true,
  "105,112,114,116,118,74,83,97,97,99" );

test();

// given a string, return a string consisting of the char value of each
// character in the string, separated by commas

function getCharValues(string) {
  for ( var c = 0, cString = ""; c < string.length; c++ ) {
    cString += string.charCodeAt(c) + ((c+1 < string.length) ? "," : "");
  }
  return cString;
}

// figure out what methods exist
// if there is no java method with the same name as a js method, should
// be able to invoke the js method without casting to a js string.  also
// the method should equal the same method of String.prototype.
// if there is a java method with the same name as a js method, invoking
// the method should call the java method

function TestObject( description, ob, method, override, expect ) {
  this.description = description;
  this.object = ob;
  this.method = method;
  this.override = override
    this.expect;

  this.result = eval(description);

  this.isJSMethod = eval( ob +"."+ method +" == Array.prototype." + method );

  // verify result of method

  new TestCase(
    description,
    expect,
    this.result );

  // verify that method is the method of Array.prototype

  new TestCase(
    ob +"." + method +" == Array.prototype." + method,
    override,
    this.isJSMethod );

  // verify that it's not cast to JS Array type

  new TestCase(
    ob + ".getClass().getName() +''",
    "[B",
    eval( ob+".getClass().getName() +''") );

}

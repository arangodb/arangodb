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

gTestfile = 'boolean-014.js';

/**
 * Preferred Argument Conversion.
 *
 * Use the syntax for explicit method invokation to override the default
 * preferred argument conversion.
 *
 */
var SECTION = "Preferred argument conversion:  boolean";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var TEST_CLASS = new
  Packages.com.netscape.javascript.qa.lc3.bool.Boolean_001;

// invoke method that accepts java.lang.Boolean

new TestCase(
  "TEST_CLASS[\"ambiguous(java.lang.Boolean)\"](true)",
  TEST_CLASS.BOOLEAN_OBJECT,
  TEST_CLASS["ambiguous(java.lang.Boolean)"](true) );

new TestCase(
  "TEST_CLASS[\"ambiguous(java.lang.Boolean)\"](false)",
  TEST_CLASS.BOOLEAN_OBJECT,
  TEST_CLASS["ambiguous(java.lang.Boolean)"](false) );

// invoke method that expects java.lang.Object

new TestCase(
  "TEST_CLASS[\"ambiguous(java.lang.Object)\"](true)",
  TEST_CLASS.OBJECT,
  TEST_CLASS["ambiguous(java.lang.Object)"](true) );

new TestCase(
  "TEST_CLASS[\"ambiguous(java.lang.Boolean)\"](false)",
  TEST_CLASS.OBJECT,
  TEST_CLASS["ambiguous(java.lang.Object)"](false) );

// invoke method that expects a String

new TestCase(
  "TEST_CLASS[\"ambiguous(java.lang.String)\"](true)",
  TEST_CLASS.STRING,
  TEST_CLASS["ambiguous(java.lang.String)"](true) );

new TestCase(
  "TEST_CLASS[\"ambiguous(java.lang.Boolean)\"](false)",
  TEST_CLASS.STRING,
  TEST_CLASS["ambiguous(java.lang.String)"](false) );

test();

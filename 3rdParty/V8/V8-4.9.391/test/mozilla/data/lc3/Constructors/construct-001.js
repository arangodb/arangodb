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

gTestfile = 'construct-001.js';

/**
 *  Verify that specific constructors can be invoked.
 *
 *
 */

var SECTION = "Explicit Constructor Invokation";
var VERSION = "JS1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Conversion";

startTest();

var DT = Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass;

new TestCase(
  "dt = new DT()" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_NONE,
  new DT().PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT(5)" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT(5).CONSTRUCTOR_ARG_DOUBLE,
  new DT(5).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT(\"true\")" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT(true).CONSTRUCTOR_ARG_BOOLEAN,
  new DT(true).PUB_INT_CONSTRUCTOR_ARG );

// force type conversion

// convert boolean

new TestCase(
  "dt = new DT[\"(boolean)\"](true)" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT("5").CONSTRUCTOR_ARG_BOOLEAN,
  new DT["(boolean)"](true).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(java.lang.Boolean)\"](true)" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_BOOLEAN_OBJECT,
  new DT["(java.lang.Boolean)"](true).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(java.lang.Object)\"](true)" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT("5").CONSTRUCTOR_ARG_OBJECT,
  new DT["(java.lang.Object)"](true).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(java.lang.String)\"](true)" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_STRING,
  new DT["(java.lang.String)"](true).PUB_INT_CONSTRUCTOR_ARG );


// convert number

new TestCase(
  "dt = new DT[\"(double)\"](5)" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT(5).CONSTRUCTOR_ARG_DOUBLE,
  new DT["(double)"]("5").PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(java.lang.Double)\"](5)" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT(5).CONSTRUCTOR_ARG_DOUBLE_OBJECT,
  new DT["(java.lang.Double)"](5).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(char)\"](5)" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_CHAR,
  new DT["(char)"](5).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(java.lang.Object)\"](5)" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_OBJECT,
  new DT["(java.lang.Object)"](5).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(java.lang.String)\"](5)" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_STRING,
  new DT["(java.lang.String)"](5).PUB_INT_CONSTRUCTOR_ARG );

// convert string

new TestCase(
  "dt = new DT(\"5\")" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT("5").CONSTRUCTOR_ARG_STRING,
  new DT("5").PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(java.lang.String)\"](\"5\")" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT("5").CONSTRUCTOR_ARG_STRING,
  new DT["(java.lang.String)"]("5").PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(char)\"](\"J\")" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_CHAR,
  new DT["(char)"]("J").PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(double)\"](\"5\")" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT("5").CONSTRUCTOR_ARG_DOUBLE,
  new DT["(double)"]("5").PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(java.lang.Object)\"](\"hello\")" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT("hello").CONSTRUCTOR_ARG_OBJECT,
  new DT["(java.lang.Object)"]("hello").PUB_INT_CONSTRUCTOR_ARG );

// convert java object

new TestCase(
  "dt = new DT[\"(java.lang.Object)\"](new java.lang.String(\"hello\")" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_OBJECT,
  new DT["(java.lang.Object)"](new java.lang.String("hello")).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(java.lang.Object)\"](new java.lang.String(\"hello\")" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_OBJECT,
  new DT["(java.lang.Object)"](new java.lang.String("hello")).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(double)\"](new java.lang.Double(5);" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_DOUBLE,
  new DT["(double)"](new java.lang.Double(5)).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(char)\"](new java.lang.Double(5);" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_CHAR,
  new DT["(char)"](new java.lang.Double(5)).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(java.lang.String)\"](new java.lang.Double(5);" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_STRING,
  new DT["(java.lang.String)"](new java.lang.Double(5)).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(java.lang.Double)\"](new java.lang.Double(5);" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_DOUBLE_OBJECT,
  new DT["(java.lang.Double)"](new java.lang.Double(5)).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT(new java.lang.Double(5);" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT().CONSTRUCTOR_ARG_DOUBLE_OBJECT,
  new DT(new java.lang.Double(5)).PUB_INT_CONSTRUCTOR_ARG );

// java array

new TestCase(
  "dt = new DT[\"(java.lang.String)\"](new java.lang.String(\"hello\").getBytes())" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT("hello").CONSTRUCTOR_ARG_STRING,
  new DT["(java.lang.String)"](new java.lang.String("hello").getBytes()).PUB_INT_CONSTRUCTOR_ARG );

new TestCase(
  "dt = new DT[\"(byte[])\"](new java.lang.String(\"hello\").getBytes())" +
  "dt.PUB_CONSTRUCTOR_ARG",
  new DT("hello").CONSTRUCTOR_ARG_BYTE_ARRAY,
  new DT["(byte[])"](new java.lang.String("hello").getBytes()).PUB_INT_CONSTRUCTOR_ARG );

test();

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

gTestfile = 'proto_1.js';

/**
   File Name:          proto_1.js
   Section:
   Description:        new PrototypeObject

   This tests Object Hierarchy and Inheritance, as described in the document
   Object Hierarchy and Inheritance in JavaScript, last modified on 12/18/97
   15:19:34 on http://devedge.netscape.com/.  Current URL:
   http://devedge.netscape.com/docs/manuals/communicator/jsobj/contents.htm

   This tests the syntax ObjectName.prototype = new PrototypeObject using the
   Employee example in the document referenced above.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "proto_1";
var VERSION = "JS1_3";
var TITLE   = "new PrototypeObject";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

function Employee () {
  this.name = "";
  this.dept = "general";
}
function Manager () {
  this.reports = [];
}
Manager.prototype = new Employee();

function WorkerBee () {
  this.projects = new Array();
}
WorkerBee.prototype = new Employee();

function SalesPerson () {
  this.dept = "sales";
  this.quota = 100;
}
SalesPerson.prototype = new WorkerBee();

function Engineer () {
  this.dept = "engineering";
  this.machine = "";
}
Engineer.prototype = new WorkerBee();

var jim = new Employee();

new TestCase( SECTION,
	      "jim = new Employee(); jim.name",
	      "",
	      jim.name );


new TestCase( SECTION,
	      "jim = new Employee(); jim.dept",
	      "general",
	      jim.dept );

var sally = new Manager();

new TestCase( SECTION,
	      "sally = new Manager(); sally.name",
	      "",
	      sally.name );
new TestCase( SECTION,
	      "sally = new Manager(); sally.dept",
	      "general",
	      sally.dept );

new TestCase( SECTION,
	      "sally = new Manager(); sally.reports.length",
	      0,
	      sally.reports.length );

new TestCase( SECTION,
	      "sally = new Manager(); typeof sally.reports",
	      "object",
	      typeof sally.reports );

var fred = new SalesPerson();

new TestCase( SECTION,
	      "fred = new SalesPerson(); fred.name",
	      "",
	      fred.name );

new TestCase( SECTION,
	      "fred = new SalesPerson(); fred.dept",
	      "sales",
	      fred.dept );

new TestCase( SECTION,
	      "fred = new SalesPerson(); fred.quota",
	      100,
	      fred.quota );

new TestCase( SECTION,
	      "fred = new SalesPerson(); fred.projects.length",
	      0,
	      fred.projects.length );

var jane = new Engineer();

new TestCase( SECTION,
	      "jane = new Engineer(); jane.name",
	      "",
	      jane.name );

new TestCase( SECTION,
	      "jane = new Engineer(); jane.dept",
	      "engineering",
	      jane.dept );

new TestCase( SECTION,
	      "jane = new Engineer(); jane.projects.length",
	      0,
	      jane.projects.length );

new TestCase( SECTION,
	      "jane = new Engineer(); jane.machine",
	      "",
	      jane.machine );


test();

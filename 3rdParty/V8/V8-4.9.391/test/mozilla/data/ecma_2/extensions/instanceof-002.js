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

gTestfile = 'instanceof-002.js';

/**
   File Name:          instanceof-002.js
   Section:
   Description:        Determining Instance Relationships

   This test is the same as js1_3/inherit/proto-002, except that it uses
   the builtin instanceof operator rather than a user-defined function
   called InstanceOf.

   This tests Object Hierarchy and Inheritance, as described in the document
   Object Hierarchy and Inheritance in JavaScript, last modified on 12/18/97
   15:19:34 on http://devedge.netscape.com/.  Current URL:
   http://devedge.netscape.com/docs/manuals/communicator/jsobj/contents.htm

   This tests the syntax ObjectName.prototype = new PrototypeObject using the
   Employee example in the document referenced above.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
//    onerror = err;

var SECTION = "instanceof-002";
var VERSION = "ECMA_2";
var TITLE   = "Determining Instance Relationships";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

function InstanceOf( object, constructor ) {
  while ( object != null ) {
    if ( object == constructor.prototype ) {
      return true;
    }
    object = object.__proto__;
  }
  return false;
}

function Employee ( name, dept ) {
  this.name = name || "";
  this.dept = dept || "general";
}

function Manager () {
  this.reports = [];
}
Manager.prototype = new Employee();

function WorkerBee ( name, dept, projs ) {
  this.base = Employee;
  this.base( name, dept)
    this.projects = projs || new Array();
}
WorkerBee.prototype = new Employee();

function SalesPerson () {
  this.dept = "sales";
  this.quota = 100;
}
SalesPerson.prototype = new WorkerBee();

function Engineer ( name, projs, machine ) {
  this.base = WorkerBee;
  this.base( name, "engineering", projs )
    this.machine = machine || "";
}
Engineer.prototype = new WorkerBee();

var pat = new Engineer();

new TestCase( SECTION,
	      "pat.__proto__ == Engineer.prototype",
	      true,
	      pat.__proto__ == Engineer.prototype );

new TestCase( SECTION,
	      "pat.__proto__.__proto__ == WorkerBee.prototype",
	      true,
	      pat.__proto__.__proto__ == WorkerBee.prototype );

new TestCase( SECTION,
	      "pat.__proto__.__proto__.__proto__ == Employee.prototype",
	      true,
	      pat.__proto__.__proto__.__proto__ == Employee.prototype );

new TestCase( SECTION,
	      "pat.__proto__.__proto__.__proto__.__proto__ == Object.prototype",
	      true,
	      pat.__proto__.__proto__.__proto__.__proto__ == Object.prototype );

new TestCase( SECTION,
	      "pat.__proto__.__proto__.__proto__.__proto__.__proto__ == null",
	      true,
	      pat.__proto__.__proto__.__proto__.__proto__.__proto__ == null );

new TestCase( SECTION,
	      "pat instanceof Engineer",
	      true,
	      pat instanceof Engineer );

new TestCase( SECTION,
	      "pat instanceof WorkerBee )",
	      true,
	      pat instanceof WorkerBee );

new TestCase( SECTION,
	      "pat instanceof Employee )",
	      true,
	      pat instanceof Employee );

new TestCase( SECTION,
	      "pat instanceof Object )",
	      true,
	      pat instanceof Object );

new TestCase( SECTION,
	      "pat instanceof SalesPerson )",
	      false,
	      pat instanceof SalesPerson );
test();

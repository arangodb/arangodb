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

gTestfile = 'proto_9.js';

/**
   File Name:          proto_9.js
   Section:
   Description:        Local versus Inherited Values

   This tests Object Hierarchy and Inheritance, as described in the document
   Object Hierarchy and Inheritance in JavaScript, last modified on 12/18/97
   15:19:34 on http://devedge.netscape.com/.  Current URL:
   http://devedge.netscape.com/docs/manuals/communicator/jsobj/contents.htm

   This tests the syntax ObjectName.prototype = new PrototypeObject using the
   Employee example in the document referenced above.

   This tests

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "proto_9";
var VERSION = "JS1_3";
var TITLE   = "Local versus Inherited Values";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

function Employee ( name, dept ) {
  this.name = name || "";
  this.dept = dept || "general";
}
function WorkerBee ( name, dept, projs ) {
  this.projects = new Array();
}
WorkerBee.prototype = new Employee();

var pat = new WorkerBee()

  Employee.prototype.specialty = "none";
Employee.prototype.name = "Unknown";

Array.prototype.getClass = Object.prototype.toString;

// Pat, the WorkerBee

new TestCase( SECTION,
	      "pat.name",
	      "",
	      pat.name );

new TestCase( SECTION,
	      "pat.dept",
	      "general",
	      pat.dept );

new TestCase( SECTION,
	      "pat.projects.getClass",
	      "[object Array]",
	      pat.projects.getClass() );

new TestCase( SECTION,
	      "pat.projects.length",
	      0,
	      pat.projects.length );

test();

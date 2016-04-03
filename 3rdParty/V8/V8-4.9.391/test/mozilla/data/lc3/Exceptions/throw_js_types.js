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

gTestfile = 'throw_js_types.js';

/**
   File Name:          throw_js_type.js
   Title:              Throw JS types as exceptions through Java
   Description:        Attempt to throw all of the basic JS primitive types
   ie. 	String
   Number
   Boolean
   Object
   as exceptions through Java. If wrapping/unwrapping
   occurs as expected, the original type will be
   preserved.

   Author:             Chris Cooper
   Email:              coop@netscape.com
   Date:               12/04/1998
*/

var SECTION = "LC3";
var TITLE   = "Throw JS types as exceptions through Java";
startTest();

var exception = "No exception thrown";
var result = "Failed";
var data_type = "no type";

/**************************
 * JS String
 *************************/
try {
  exception = Packages.com.netscape.javascript.qa.liveconnect.JSObjectEval.eval(this, "throw 'foo';");
} catch ( e ) {
  exception = e.toString();
  data_type = typeof e;
  if (exception == "foo")
    result = "Passed!";
}

new TestCase("Throwing JS string through Java "+
	     "\n=> threw ("+data_type+") "+exception+" ",
	     "Passed!",
	     result );

/**************************
 * JS Number (int)
 *************************/
exception = "No exception thrown";
result = "Failed";
data_type = "no type";

try {
  exception = Packages.com.netscape.javascript.qa.liveconnect.JSObjectEval.eval(this, "throw 42;");
} catch ( e ) {
  exception = e.toString();
  data_type = typeof e;
  if (exception == "42")
    result = "Passed!";
}

new TestCase("Throwing JS number (int) through Java "+
	     "\n=> threw ("+data_type+") "+exception+" ",
	     "Passed!",
	     result );

/**************************
 * JS Number (float)
 *************************/
exception = "No exception thrown";
result = "Failed";
data_type = "no type";

try {
  exception = Packages.com.netscape.javascript.qa.liveconnect.JSObjectEval.eval(this, "throw 4.2;");
} catch ( e ) {
  exception = e.toString();
  data_type = typeof e;
  if (exception == "4.2")
    result = "Passed!";
}

new TestCase("Throwing JS number (float) through Java "+
	     "\n=> threw ("+data_type+") "+exception+" ",
	     "Passed!",
	     result );

/**************************
 * JS Boolean
 *************************/
exception = "No exception thrown";
result = "Failed";
data_type = "no type";

try {
  exception = Packages.com.netscape.javascript.qa.liveconnect.JSObjectEval.eval(this, "throw false;");
} catch ( e ) {
  exception = e.toString();
  data_type = typeof e;
  if (exception == "false")
    result = "Passed!";
}

new TestCase("Throwing JS boolean through Java "+
	     "\n=> threw ("+data_type+") "+exception+" ",
	     "Passed!",
	     result );

/**************************
 * JS Object
 *************************/
exception = "No exception thrown";
result = "Failed";
data_type = "no type";

try {
  exception = Packages.com.netscape.javascript.qa.liveconnect.JSObjectEval.eval(this, "throw {a:5};");
} catch ( e ) {
  exception = e.toString();
  data_type = typeof e;
  result = "Passed!";
}

new TestCase("Throwing JS Object through Java "+
	     "\n=> threw ("+data_type+") "+exception+" ",
	     "Passed!",
	     result );

/**************************
 * JS Object (Date)
 *************************/
exception = "No exception thrown";
result = "Failed";
data_type = "no type";

try {
  exception = Packages.com.netscape.javascript.qa.liveconnect.JSObjectEval.eval(this, "throw new Date();");
} catch ( e ) {
  exception = e.toString();
  data_type = typeof e;
  result = "Passed!";
}

new TestCase("Throwing JS Object (Date)through Java "+
	     "\n=> threw ("+data_type+") "+exception+" ",
	     "Passed!",
	     result );

/**************************
 * JS Object (String)
 *************************/
exception = "No exception thrown";
result = "Failed";
data_type = "no type";

try {
  exception = Packages.com.netscape.javascript.qa.liveconnect.JSObjectEval.eval(this, "throw new String();");
} catch ( e ) {
  exception = e.toString();
  data_type = typeof e;
  result = "Passed!";
}

new TestCase("Throwing JS Object (String) through Java "+
	     "\n=> threw ("+data_type+") "+exception+" ",
	     "Passed!",
	     result );

/**************************
 * Undefined
 *************************/
exception = "No exception thrown";
result = "Failed";
data_type = "no type";

try {
  exception = Packages.com.netscape.javascript.qa.liveconnect.JSObjectEval.eval(this, "throw undefined");
} catch ( e ) {
  exception = "Exception";
  data_type = typeof e;
  result = "Passed!";
}

new TestCase("Throwing undefined through Java "+
	     "\n=> threw ("+data_type+") "+exception+" ",
	     "Passed!",
	     result );

/**************************
 * null
 *************************/
exception = "No exception thrown";
result = "Failed";
data_type = "no type";

try {
  exception = Packages.com.netscape.javascript.qa.liveconnect.JSObjectEval.eval(this, "throw null;");
} catch ( e ) {
  exception = "Exception";
  data_type = typeof e;
  result = "Passed!";
}

new TestCase("Throwing null through Java "+
	     "\n=> threw ("+data_type+") "+exception+" ",
	     "Passed!",
	     result );

test();



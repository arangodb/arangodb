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

gTestfile = '12.5-2.js';

/**
   File Name:          12.5-2.js
   ECMA Section:       The if statement
   Description:

   The production IfStatement : if ( Expression ) Statement else Statement
   is evaluated as follows:

   1.Evaluate Expression.
   2.Call GetValue(Result(1)).
   3.Call ToBoolean(Result(2)).
   4.If Result(3) is false, go to step 7.
   5.Evaluate the first Statement.
   6.Return Result(5).
   7.Evaluate the second Statement.
   8.Return Result(7).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "12.5-2";
var VERSION = "ECMA_1";
startTest();
var TITLE = "The if statement" ;

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(   SECTION,
		"var MYVAR; if ( true ) MYVAR='PASSED'; MYVAR",
		"PASSED",
		eval("var MYVAR; if ( true ) MYVAR='PASSED'; MYVAR") );

new TestCase(  SECTION,
	       "var MYVAR; if ( false ) MYVAR='FAILED'; MYVAR;",
	       "PASSED",
	       eval("var MYVAR=\"PASSED\"; if ( false ) MYVAR='FAILED'; MYVAR;") );

new TestCase(   SECTION,
		"var MYVAR; if ( new Boolean(true) ) MYVAR='PASSED'; MYVAR",
		"PASSED",
		eval("var MYVAR; if ( new Boolean(true) ) MYVAR='PASSED'; MYVAR") );

new TestCase(   SECTION,
		"var MYVAR; if ( new Boolean(false) ) MYVAR='PASSED'; MYVAR",
		"PASSED",
		eval("var MYVAR; if ( new Boolean(false) ) MYVAR='PASSED'; MYVAR") );

new TestCase(   SECTION,
		"var MYVAR; if ( 1 ) MYVAR='PASSED'; MYVAR",
		"PASSED",
		eval("var MYVAR; if ( 1 ) MYVAR='PASSED'; MYVAR") );

new TestCase(  SECTION,
	       "var MYVAR; if ( 0 ) MYVAR='FAILED'; MYVAR;",
	       "PASSED",
	       eval("var MYVAR=\"PASSED\"; if ( 0 ) MYVAR='FAILED'; MYVAR;") );

test();

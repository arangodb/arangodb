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

gTestfile = '10.1.3.js';

/**
   File Name:          10.1.3.js
   ECMA Section:       10.1.3.js Variable Instantiation
   Description:
   Author:             christine@netscape.com
   Date:               11 september 1997
*/

var SECTION = "10.1.3";
var VERSION = "ECMA_1";
var TITLE   = "Variable instantiation";
var BUGNUMBER = "20256";
startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

   
// overriding a variable or function name with a function should succeed
    
new TestCase(SECTION,
	     "function t() { return \"first\" };" +
	     "function t() { return \"second\" };t() ",
	     "second",
	     eval("function t() { return \"first\" };" +
		  "function t() { return \"second\" };t()"));

   
new TestCase(SECTION,
	     "var t; function t(){}; typeof(t)",
	     "function",
	     eval("var t; function t(){}; typeof(t)"));


// formal parameter tests
    
new TestCase(SECTION,
	     "function t1(a,b) { return b; }; t1( 4 );",
	     void 0,
	     eval("function t1(a,b) { return b; }; t1( 4 );") );
   
new TestCase(SECTION,
	     "function t1(a,b) { return a; }; t1(4);",
	     4,
	     eval("function t1(a,b) { return a; }; t1(4)"));
    
new TestCase(SECTION,
	     "function t1(a,b) { return a; }; t1();",
	     void 0,
	     eval("function t1(a,b) { return a; }; t1()"));
   
new TestCase(SECTION,
	     "function t1(a,b) { return a; }; t1(1,2,4);",
	     1,
	     eval("function t1(a,b) { return a; }; t1(1,2,4)"));
/*
   
new TestCase(SECTION, "function t1(a,a) { return a; }; t1( 4 );",
void 0,
eval("function t1(a,a) { return a; }; t1( 4 )"));
    
new TestCase(SECTION,
"function t1(a,a) { return a; }; t1( 1,2 );",
2,
eval("function t1(a,a) { return a; }; t1( 1,2 )"));
*/
// variable declarations
   
new TestCase(SECTION,
	     "function t1(a,b) { return a; }; t1( false, true );",
	     false,
	     eval("function t1(a,b) { return a; }; t1( false, true );"));
   
new TestCase(SECTION,
	     "function t1(a,b) { return b; }; t1( false, true );",
	     true,
	     eval("function t1(a,b) { return b; }; t1( false, true );"));
   
new TestCase(SECTION,
	     "function t1(a,b) { return a+b; }; t1( 4, 2 );",
	     6,
	     eval("function t1(a,b) { return a+b; }; t1( 4, 2 );"));
   
new TestCase(SECTION,
	     "function t1(a,b) { return a+b; }; t1( 4 );",
	     Number.NaN,
	     eval("function t1(a,b) { return a+b; }; t1( 4 );"));

// overriding a function name with a variable should fail
   
new TestCase(SECTION,
	     "function t() { return 'function' };" +
	     "var t = 'variable'; typeof(t)",
	     "string",
	     eval("function t() { return 'function' };" +
		  "var t = 'variable'; typeof(t)"));

// function as a constructor
   
new TestCase(SECTION,
	     "function t1(a,b) { var a = b; return a; } t1(1,3);",
	     3,
	     eval("function t1(a, b){ var a = b; return a;}; t1(1,3)"));
   
new TestCase(SECTION,
	     "function t2(a,b) { this.a = b;  } x  = new t2(1,3); x.a",
	     3,
	     eval("function t2(a,b) { this.a = b; };" +
		  "x = new t2(1,3); x.a"));
   
new TestCase(SECTION,
	     "function t2(a,b) { this.a = a;  } x  = new t2(1,3); x.a",
	     1,
	     eval("function t2(a,b) { this.a = a; };" +
		  "x = new t2(1,3); x.a"));
   
new TestCase(SECTION,
	     "function t2(a,b) { this.a = b; this.b = a; } " +
	     "x = new t2(1,3);x.a;",
	     3,
	     eval("function t2(a,b) { this.a = b; this.b = a; };" +
		  "x = new t2(1,3);x.a;"));
   
new TestCase(SECTION,
	     "function t2(a,b) { this.a = b; this.b = a; }" +
	     "x = new t2(1,3);x.b;",
	     1,
	     eval("function t2(a,b) { this.a = b; this.b = a; };" +
		  "x = new t2(1,3);x.b;") );

test();

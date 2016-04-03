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

gTestfile = '11.12-1.js';

/**
   File Name:          11.12.js
   ECMA Section:       11.12 Conditional Operator
   Description:
   Logi

   calORExpression ? AssignmentExpression : AssignmentExpression

   Semantics

   The production ConditionalExpression :
   LogicalORExpression ? AssignmentExpression : AssignmentExpression
   is evaluated as follows:

   1.  Evaluate LogicalORExpression.
   2.  Call GetValue(Result(1)).
   3.  Call ToBoolean(Result(2)).
   4.  If Result(3) is false, go to step 8.
   5.  Evaluate the first AssignmentExpression.
   6.  Call GetValue(Result(5)).
   7.  Return Result(6).
   8.  Evaluate the second AssignmentExpression.
   9.  Call GetValue(Result(8)).
   10.  Return Result(9).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.12";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Conditional operator( ? : )");

new TestCase( SECTION,   
              "true ? 'PASSED' : 'FAILED'",    
              "PASSED",      
              (true?"PASSED":"FAILED"));

new TestCase( SECTION,   
              "false ? 'FAILED' : 'PASSED'",    
              "PASSED",     
              (false?"FAILED":"PASSED"));

new TestCase( SECTION,   
              "1 ? 'PASSED' : 'FAILED'",    
              "PASSED",         
              (true?"PASSED":"FAILED"));

new TestCase( SECTION,   
              "0 ? 'FAILED' : 'PASSED'",    
              "PASSED",         
              (false?"FAILED":"PASSED"));

new TestCase( SECTION,   
              "-1 ? 'PASSED' : 'FAILED'",    
              "PASSED",         
              (true?"PASSED":"FAILED"));

new TestCase( SECTION,   
              "NaN ? 'FAILED' : 'PASSED'",    
              "PASSED",         
              (Number.NaN?"FAILED":"PASSED"));

new TestCase( SECTION,   
              "var VAR = true ? , : 'FAILED'",
              "PASSED",          
              (VAR = true ? "PASSED" : "FAILED") );

test();

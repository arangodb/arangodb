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
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   igor@icesoft.no, pschwartau@netscape.com
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

/*
 *
 * Date:    21 February 2003
 * SUMMARY: Testing eval statements containing conditional function expressions
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=194364
 *
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-194364.js';
var UBound = 0;
var BUGNUMBER = 194364;
var summary = 'Testing eval statements with conditional function expressions';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

/*
  From ECMA-262 Edition 3, 12.4:

  12.4 Expression Statement

  Syntax
  ExpressionStatement : [lookahead not in {{, function}] Expression ;

  Note that an ExpressionStatement cannot start with an opening curly brace
  because that might make it ambiguous with a Block. Also, an ExpressionStatement
  cannot start with the function keyword because that might make it ambiguous with
  a FunctionDeclaration.
*/

status = inSection(1);
actual = eval('(function() {}); 1');
expect = 1;
addThis();

status = inSection(2);
actual = eval('(function f() {}); 2');
expect = 2;
addThis();

status = inSection(3);
actual = eval('if (true) (function() {}); 3');
expect = 3;
addThis();

status = inSection(4);
actual = eval('if (true) (function f() {}); 4');
expect = 4;
addThis();

status = inSection(5);
actual = eval('if (false) (function() {}); 5');
expect = 5;
addThis();

status = inSection(6);
actual = eval('if (false) (function f() {}); 6');
expect = 6;
addThis();

status = inSection(7);
actual = eval('switch(true) { case true: (function() {}) }; 7');
expect = 7;
addThis();

status = inSection(8);
actual = eval('switch(true) { case true: (function f() {}) }; 8');
expect = 8;
addThis();

status = inSection(9);
actual = eval('switch(false) { case false: (function() {}) }; 9');
expect = 9;
addThis();

status = inSection(10);
actual = eval('switch(false) { case false: (function f() {}) }; 10');
expect = 10;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}

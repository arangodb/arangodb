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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Brendan Eich
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

var gTestfile = 'regress-164697.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 164697;
var summary = '(instance.__parent__ == constructor.__parent__)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = 'true';

runtest('{}', 'Object');
runtest('new Object()', 'Object');

// see https://bugzilla.mozilla.org/show_bug.cgi?id=321669
// for why this test is not contained in a function.
actual = (function (){}).__proto__ == Function.prototype;
reportCompare('true', actual+'',
              '(function (){}).__proto__ == Function.prototype');

runtest('new Function(";")', 'Function');

runtest('[]', 'Array');
runtest('new Array()', 'Array');

runtest('""', 'String');
runtest('new String()', 'String');

runtest('true', 'Boolean');
runtest('new Boolean()', 'Boolean');

runtest('1', 'Number');
runtest('new Number("1")', 'Number');

runtest('new Date()', 'Date');

runtest('/x/', 'RegExp');
runtest('new RegExp("x")', 'RegExp');

runtest('new Error()', 'Error');

function runtest(myinstance, myconstructor)
{
  var expr;
  var actual;

  try
  {
    expr =  '(' + myinstance + ').__parent__ == ' +
      myconstructor + '.__parent__';
    printStatus(expr);
    actual = eval(expr).toString();
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, expr);

  try
  {
    expr =  '(' + myinstance + ').__proto__ == ' +
      myconstructor + '.prototype';
    printStatus(expr);
    actual = eval(expr).toString();
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, expr);
}

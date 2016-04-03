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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Biju
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

var gTestfile = 'regress-333541.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 333541;
var summary = '1..toSource()';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
function a(){
  return 1..toSource();
}

try
{
  expect = 'function a() {\n    return (1).toSource();\n}';
  actual = a.toString();
  compareSource(expect, actual, summary + ': 1');
}
catch(ex)
{
  actual = ex + '';
  reportCompare(expect, actual, summary + ': 1');
}

try
{
  expect = 'function a() {return (1).toSource();}';
  actual = a.toSource();
  compareSource(expect, actual, summary + ': 2');
}
catch(ex)
{
  actual = ex + '';
  reportCompare(expect, actual, summary + ': 2');
}

expect = a;
actual = a.valueOf();
reportCompare(expect, actual, summary + ': 3');

try
{
  expect = 'function a() {\n    return (1).toSource();\n}';
  actual = "" + a;
  compareSource(expect, actual, summary + ': 4');
}
catch(ex)
{
  actual = ex + '';
  reportCompare(expect, actual, summary + ': 4');
}

function b(){
  x=1..toSource();
  x=1['a'];
  x=1..a;
  x=1['"a"'];
  x=1["'a'"];
  x=1['1'];
  x=1["#"];
}

try
{
  expect = "function b() {\n    x = (1).toSource();\n" +
    "    x = (1).a;\n" +
    "    x = (1).a;\n" +
    "    x = (1)['\"a\"'];\n" +
    "    x = (1)[\'\\'a\\''];\n" +
    "    x = (1)['1'];\n" +
    "    x = (1)['#'];\n" +
    "}";
  actual = "" + b;
  // fudge the actual to match a['1'] ~ a[1].
  // see https://bugzilla.mozilla.org/show_bug.cgi?id=452369
  actual = actual.replace(/\(1\)\[1\];/, "(1)['1'];");
  compareSource(expect, actual, summary + ': 5');
}
catch(ex)
{
  actual = ex + '';
  reportCompare(expect, actual, summary + ': 5');
}


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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Igor Bukanov
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

var gTestfile = 'regress-394673.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 394673;
var summary = 'Parsing long chains of "&&" or "||"';
var actual = 'No Error';
var expect = 'No Error';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var N = 70*1000;
var counter;

counter = 0;
var x = build("&&")();
if (x !== true)
  throw "Unexpected result: x="+x;
if (counter != N)
  throw "Unexpected counter: counter="+counter;

counter = 0;
var x = build("||")();
if (x !== true)
  throw "Unexpected result: x="+x;
if (counter != 1)
  throw "Unexpected counter: counter="+counter;

function build(operation)
{
  var counter;
  var a = [];
  a.push("return f()");
  for (var i = 1; i != N - 1; ++i)
    a.push("f()");
  a.push("f();");
  return new Function(a.join(operation));
}

function f()
{
  ++counter;
  return true;
}

reportCompare(expect, actual, summary);

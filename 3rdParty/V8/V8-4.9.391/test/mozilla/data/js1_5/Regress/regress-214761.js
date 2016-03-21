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
 * Contributor(s): Olivier Cahagne
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

var gTestfile = 'regress-214761.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 214761;
var summary = 'Crash Regression from bug 208030';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

options('strict');

var code = "var bar1=new Array();\n" +
  "bar1[0]='foo';\n" +
  "var bar2=document.all&&navigator.userAgent.indexOf('Opera')==-1;\n" +
  "var bar3=document.getElementById&&!document.all;\n" +
  "var bar4=document.layers;\n" +
  "function foo1(){\n" +
  "return false;}\n" +
  "function foo2(){\n" +
  "return false;}\n" +
  "function foo3(){\n" +
  "return false;}\n" +
  "function foo4(){\n" +
  "return false;}\n" +
  "function foo5(){\n" +
  "return false;}\n" +
  "function foo6(){\n" +
  "return false;}\n" +
  "function foo7(){\n" +
  "return false;}";

try
{
  eval(code);
}
catch(e)
{
}

reportCompare(expect, actual, summary);

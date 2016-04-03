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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Gary Kwong
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

var gTestfile = 'regress-476414-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 476414;
var summary = 'Do not crash @ GetGCThingFlags';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function whatToTestSpidermonkeyTrunk(code)
{
  return {
  allowExec: true
      };
}
whatToTest = whatToTestSpidermonkeyTrunk;
function tryItOut(code)
{
  var wtt = whatToTest(code.replace(/\n/g, " ").replace(/\r/g, " "));
  var f = new Function(code);
  if (wtt.allowExec && f) {
    rv = tryRunning(f, code);
  }
}
function tryRunning(f, code)
{
  try { 
    var rv = f();
  } catch(runError) {}
}
var realFunction = Function;
var realUneval = uneval;
var realToString = toString;
var realToSource = toSource;
function tryEnsureSanity()
{
  delete Function;
  delete uneval;
  delete toSource;
  delete toString;
  Function = realFunction;
  uneval = realUneval;
  toSource = realToSource;
  toString = realToString;
}
for (let iters = 0; iters < 2000; ++iters) { 
  count=27745; tryItOut("with({x: (c) = (x2 = [])})false;");
  tryEnsureSanity();
  count=35594; tryItOut("switch(null) { case this.__defineSetter__(\"window\", function () { yield  \"\"  } ): break; }");
  tryEnsureSanity();
  count=45020; tryItOut("with({}) { (this.__defineGetter__(\"x\", function (y)this)); } ");
  tryEnsureSanity();
  count=45197; tryItOut("M:with((p={}, (p.z = <x/> ===  '' )()))/*TUUL*/for each (let y in [true, {}, {}, (void 0), true, true, true, (void 0), true, (void 0)]) { return; }");
  tryEnsureSanity();
  gc();
  tryEnsureSanity();
  count=45254; tryItOut("for each (NaN in this);");
  tryEnsureSanity();
}
 
reportCompare(expect, actual, summary);

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

var gTestfile = 'regress-452498-082.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

// ------- Comment #82 From Gary Kwong [:nth10sd]

// =====

  (function(){function  x(){} {let x = [] }});

// =====

  var f = new Function("new function x(){ return x |= function(){} } ([], function(){})");
  "" + f;

// =====

  var f = new Function("for(let [] = [0]; (y) = *; new (*::*)()) {}");
  "" + f;

// =====

  uneval(function(){[y] = [x];});

// =====

  function g(code)
  {
    var f = new Function(code);
    f();
  }
  g("for (var x = 0; x < 3; ++x)(new (function(){})());");

// =====

  try
  {
    (function(){new (function ({}, x) { yield (x(1e-81) for (x4 in undefined)) })()})();
  }
  catch(ex)
  {
  }
// =====

  try
  {
    (function(){[(function ([y]) { })() for each (x in [])];})();
  }
  catch(ex)
  {
  }
// =====

  try
  {
    eval('(function(){for(var x2 = [function(id) { return id } for each (x in []) if ([])] in functional) function(){};})();');
  }
  catch(ex)
  {
  }

// =====

  if (typeof window == 'undefined')
    global = this;
  else
    global = window;

  try
  {
    eval('(function(){with(global){1e-81; }for(let [x, x3] = global -= x in []) function(){}})();');
  }
  catch(ex)
  {
  }

// =====
  try
  {
    eval(
      'for(let [\n' +
      'function  x () { M:if([1,,])  }\n'
      );
  }
  catch(ex)
  {
  }

// =====

  try
  {
    function foo(code)
    {
      var c;
      eval("const c, x5 = c;");
    }
    foo();
  }
  catch(ex)
  {
  }

// =====

  var f = new Function("try { with({}) x = x; } catch(\u3056 if (function(){x = x2;})()) { let([] = [1.2e3.valueOf(\"number\")]) ((function(){})()); } ");
  "" + f;

// =====

  var f = new Function("[] = [( '' )()];");
  "" + f;

// =====

  var f = new Function("let ([] = [({ get x5 this (x) {}  })]) { for(let y in []) with({}) {} }");
  "" + f;

// =====

  try
  {
    eval(
	  'for(let x;' +
	  '    ([,,,]' +
	  '     .toExponential(new Function(), (function(){}))); [] = {})' +
	  '  for(var [x, x] = * in this.__defineSetter__("", function(){}));'
      );
  }
  catch(ex)
  {
  }

// =====

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

03/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

var gTestfile = 'regress-452498-117.js';
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

// ------- Comment #117 From Gary Kwong [:nth10sd]

// The following all do not require -j.

// =====

  try
  {
    eval('x; function  x(){}; const x;');
  }
  catch(ex)
  {
  }

// Assertion failure: !pn->isPlaceholder(), at ../jsparse.cpp:4876
// =====
  (function(){ var x; eval("var x; x = null"); })()

// Assertion failure: regs.sp == StackBase(fp), at ../jsinterp.cpp:2984
// =====
    function this ({x}) { function x(){} }

// Assertion failure: cg->stackDepth == stackDepth, at ../jsemit.cpp:3664
// =====
  for(let x = [ "" for (y in /x/g ) if (x)] in (""));

// Assertion failure: !(pnu->pn_dflags & PND_BOUND), at ../jsemit.cpp:1818
// =====
  (function(){const x = 0, y = delete x;})()

// Assertion failure: JOF_OPTYPE(op) == JOF_ATOM, at ../jsemit.cpp:1710
// =====
    try
    {
      (function(){(yield []) (function(){with({}){x} }); const x;})();
    }
    catch(ex)
    {
    }

// Assertion failure: cg->upvars.lookup(atom), at ../jsemit.cpp:2022
// =====
  try
  {
    (function(){([]) ((function(q) { return q; })for (each in *))})();
  }
  catch(ex)
  {
  }
// Assertion failure: lexdep->frameLevel() <= funbox->level, at ../jsparse.cpp:1782
// Opt crash [@ JSCompiler::setFunctionKinds] near null
// =====

  try
  {
    eval("((x1) > [(x)(function() { x;}) for each (x in x)])()");
  }
  catch(ex)
  {
  }

// Assertion failure: pnu->pn_lexdef == dn, at ../jsemit.cpp:1817
// =====
  uneval(function(){for(var [arguments] = ({ get y(){} }) in y ) (x);});

// Assertion failure: slot < StackDepth(jp->script), at ../jsopcode.cpp:1318
// =====
  uneval(function(){([] for ([,,]in <><y/></>));});

// Assertion failure: n != 0, at ../jsfun.cpp:2689
// =====
  try
  {
    eval('(function(){{for(c in (function (){ for(x in (x1))window} )()) {const x;} }})();');
  }
  catch(ex)
  {
  }

// Assertion failure: op == JSOP_GETLOCAL, at ../jsemit.cpp:4557
// =====
  try
  {
    (eval("(function(){let x , x =  (x for (x in null))});"))();
  }
  catch(ex)
  {
  }

// Assertion failure: (fun->u.i.script)->upvarsOffset != 0, at ../jsfun.cpp:1537
// Opt crash [@ js_NewFlatClosure] near null
// =====
  "" + function(){for(var [x] in x1) ([]); function x(){}}

// Assertion failure: cg->stackDepth == stackDepth, at ../jsemit.cpp:3664
// Opt crash [@ JS_ArenaRealloc] near null
// =====
  try
  {
    eval(
      "for (a in (function(){" +
      "      for each (let x in ['']) { return new function x1 (\u3056) { yield x } ();" +
      "        }})())" +
      "  function(){}"
      );
  }
  catch(ex)
  {
  }
// Crash [@ js_Interpret] near null
// =====

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

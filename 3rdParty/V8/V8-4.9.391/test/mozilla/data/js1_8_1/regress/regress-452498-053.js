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
 * Contributor(s): Jason Orendorff
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

var gTestfile = 'regress-452498-053.js';
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

// ------- Comment #53 From Jason Orendorff

// Assertion failure: (slot) < (uint32)(obj)->dslots[-1]
// at ../jsobj.cpp:5559
// On the last line of BindLet, we have
//    JS_SetReservedSlot(cx, blockObj, index, PRIVATE_TO_JSVAL(pn));
// but this uses reserved slots as though they were unlimited.
// blockObj only has 2.
  let (a=0, b=1, c=2) {}

// In RecycleTree at ../jsparse.cpp:315, we hit
//     JS_NOT_REACHED("RecycleUseDefKids");
// pn->pn_type is TOK_UNARYOP
// pn->pn_op   is JSOP_XMLNAME
// pn->pn_defn is 1
// pn->pn_used is 1
  try
  {
    @foo; 0;
  }
  catch(ex)
  {
  }
// Calls LinkUseToDef with pn->pn_defn == 1.
//
// If you say "var x;" first, then run this case, it gets further,
// crashing in NoteLValue like the first case in comment 52.
//
  try
  {
    for (var [x] = x in y) var x;
  }
  catch(ex)
  {
  }
// Assertion failure: !pn2->pn_defn, at ../jsparse.h:461
// Another case where some optimization is going on.
  try
  {
    if (true && @foo) ;
  }
  catch(ex)
  {
  }
// Assertion failure: scope->object == ctor
// in js_FastNewObject at ../jsbuiltins.cpp:237
//
// With the patch, we're new-ing a different function each time, and the
// .prototype property is missing.
//
  for (var z = 0; z < 3; z++) { (new function(){}); }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

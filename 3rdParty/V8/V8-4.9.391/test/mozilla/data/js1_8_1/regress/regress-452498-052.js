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

var gTestfile = 'regress-452498-052.js';
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

// ------- Comment #52 From Jason Orendorff

// Crash in NoteLValue, called from BindDestructuringVar.
// NoteLValue assumes pn->pn_lexdef is non-null, but here
// pn is itself the definition of x.
  for (var [x]=0 in null) ;

// This one only crashes when executed from a file.
// Assertion failure: pn != dn->dn_uses, at ../jsparse.cpp:1131
  for (var f in null)
    ;
  var f = 1;
  (f)

// Assertion failure: pnu->pn_cookie == FREE_UPVAR_COOKIE, at ../jsemit.cpp:1815
// In EmitEnterBlock. x has one use, which is pnu here.
// pnu is indeed a name, but pnu->pn_cookie is 0.
    try { eval('let (x = 1) { var x; }'); } catch(ex) {}

// Assertion failure: cg->upvars.lookup(atom), at ../jsemit.cpp:1992
// atom="x", upvars is empty.
  (1 for each (x in x));

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

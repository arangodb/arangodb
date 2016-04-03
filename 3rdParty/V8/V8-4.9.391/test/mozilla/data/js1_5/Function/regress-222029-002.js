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
 *   brendan@mozilla.org, pschwartau@netscape.com
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
 * Date:    13 Oct 2003
 * SUMMARY: Make our f.caller property match IE's wrt f.apply and f.call
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=222029
 *
 * Below, when gg calls f via |f.call|, we have this call chain:
 *
 *          calls                                  calls
 *   gg() --------->  Function.prototype.call()  --------->  f()
 *
 *
 * The question this bug addresses is, "What should we say |f.caller| is?"
 *
 * Before this fix, SpiderMonkey said it was |Function.prototype.call|.
 * After this fix, SpiderMonkey emulates IE and says |gg| instead.
 *
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-222029-002.js';
var UBound = 0;
var BUGNUMBER = 222029;
var summary = "Make our f.caller property match IE's wrt f.apply and f.call";
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

/*
 * Try to confuse the engine by adding a |p| property to everything!
 */
var p = 'global';
var o = {p:'object'};


function f(obj)
{
  return f.caller.p ;
}


/*
 * Call |f| directly
 */
function g(obj)
{
  var p = 'local';
  return f(obj);
}
g.p = "hello";


/*
 * Call |f| via |f.call|
 */
function gg(obj)
{
  var p = 'local';
  return f.call(obj, obj);
}
gg.p = "hello";


/*
 * Call |f| via |f.apply|
 */
function ggg(obj)
{
  var p = 'local';
  return f.apply(obj, [obj]);
}
ggg.p = "hello";


/*
 * Shadow |p| on |Function.prototype.call|, |Function.prototype.apply|.
 * In Sections 2 and 3 below, we no longer expect to recover this value -
 */
Function.prototype.call.p = "goodbye";
Function.prototype.apply.p = "goodbye";



status = inSection(1);
actual = g(o);
expect = "hello";
addThis();

status = inSection(2);
actual = gg(o);
expect = "hello";
addThis();

status = inSection(3);
actual = ggg(o);
expect = "hello";
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

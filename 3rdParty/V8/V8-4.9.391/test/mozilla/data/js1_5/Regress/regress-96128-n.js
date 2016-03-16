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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   jband@netscape.com, pschwartau@netscape.com
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
 * Date: 29 Aug 2001
 *
 * SUMMARY: Negative test that JS infinite recursion protection works.
 * We expect the code here to fail (i.e. exit code 3), but NOT crash.
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=96128
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-96128-n.js';
var BUGNUMBER = 96128;
var summary = 'Testing that JS infinite recursion protection works';


function objRecurse()
{
  /*
   * jband:
   *
   * Causes a stack overflow crash in debug builds of both the browser
   * and the shell. In the release builds this is safely caught by the
   * "too much recursion" mechanism. If I remove the 'new' from the code below
   * this is safely caught in both debug and release builds. The 'new' causes a
   * lookup for the Constructor name and seems to (at least) double the number
   * of items on the C stack for the given interpLevel depth.
   */
  return new objRecurse();
}



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  // we expect this to fail (exit code 3), but NOT crash. -
  var obj = new objRecurse();

  exitFunc ('test');
}

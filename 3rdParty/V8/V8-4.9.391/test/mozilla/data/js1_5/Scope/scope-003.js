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
 *   coliver@mminternet.com, pschwartau@netscape.com
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
 * Date: 2001-07-03
 *
 * SUMMARY:  Testing scope with nested functions
 *
 * From correspondence with Christopher Oliver <coliver@mminternet.com>:
 *
 * > Running this test with Rhino produces the following exception:
 * >
 * > uncaught JavaScript exception: undefined: Cannot find default value for
 * > object. (line 3)
 * >
 * > This is due to a bug in org.mozilla.javascript.NativeCall which doesn't
 * > implement toString or valueOf or override getDefaultValue.
 * > However, even after I hacked in an implementation of getDefaultValue in
 * > NativeCall, Rhino still produces a different result then SpiderMonkey:
 * >
 * > [object Call]
 * > [object Object]
 * > [object Call]
 *
 * Note the results should be:
 *
 *   [object global]
 *   [object Object]
 *   [object global]
 *
 * This is what we are checking for in this testcase -
 */
//-----------------------------------------------------------------------------
var gTestfile = 'scope-003.js';
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing scope with nested functions';
var statprefix = 'Section ';
var statsuffix = ' of test -';
var self = this; // capture a reference to the global object;
var cnGlobal = self.toString();
var cnObject = (new Object).toString();
var statusitems = [];
var actualvalues = [];
var expectedvalues = [];


function a()
{
  function b()
  {
    capture(this.toString());
  }

  this.c = function()
    {
      capture(this.toString());
      b();
    }

  b();
}


var obj = new a();  // captures actualvalues[0]
obj.c();            // captures actualvalues[1], actualvalues[2]


// The values we expect - see introduction above -
expectedvalues[0] = cnGlobal;
expectedvalues[1] = cnObject;
expectedvalues[2] = cnGlobal;



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function capture(val)
{
  actualvalues[UBound] = val;
  statusitems[UBound] = getStatus(UBound);
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}


function getStatus(i)
{
  return statprefix + i + statsuffix;
}

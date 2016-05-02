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
 *   pschwartau@netscape.com
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
 * Date: 2001-07-13
 *
 * SUMMARY: Applying Function.prototype.call to the Function object itself
 *
 *
 * ECMA-262 15.3.4.4 Function.prototype.call (thisArg [,arg1 [,arg2,…] ] )
 *
 * When applied to the Function object itself, thisArg should be ignored.
 * As explained by Waldemar (waldemar@netscape.com):
 *
 * Function.call(obj, "print(this)") is equivalent to invoking
 * Function("print(this)") with this set to obj. Now, Function("print(this)")
 * is equivalent to new Function("print(this)") (see 15.3.1.1), and the latter
 * ignores the this value that you passed it and constructs a function
 * (which we'll call F) which will print the value of the this that will be
 * passed in when F will be invoked.
 *
 * With the last set of () you're invoking F(), which means you're calling it
 * with no this value. When you don't provide a this value, it defaults to the
 * global object.
 *
 */

//-----------------------------------------------------------------------------
var gTestfile = 'call-001.js';
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Applying Function.prototype.call to the Function object itself';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var self = this; // capture a reference to the global object
var cnOBJECT_GLOBAL = self.toString();
var cnOBJECT_OBJECT = (new Object).toString();
var cnHello = 'Hello';
var cnRed = 'red';
var objTEST = {color:cnRed};
var f = new Function();
var g = new Function();


f = Function.call(self, 'return cnHello');
g = Function.call(objTEST, 'return cnHello');

status = 'Section A of test';
actual = f();
expect = cnHello;
captureThis();

status = 'Section B of test';
actual = g();
expect = cnHello;
captureThis();


f = Function.call(self, 'return this.toString()');
g = Function.call(objTEST, 'return this.toString()');

status = 'Section C of test';
actual = f();
expect = cnOBJECT_GLOBAL;
captureThis();

status = 'Section D of test';
actual = g();
expect = cnOBJECT_GLOBAL;
captureThis();


f = Function.call(self, 'return this.color');
g = Function.call(objTEST, 'return this.color');

status = 'Section E of test';
actual = f();
expect = undefined;
captureThis();

status = 'Section F of test';
actual = g();
expect = undefined;
captureThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function captureThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Ginda rginda@netscape.com
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

var gTestfile = '10.1.4-1.js';

/**
   ECMA Section: 10.1.4.1 Entering An Execution Context
   ECMA says:
   * Global Code, Function Code
   Variable instantiation is performed using the global object as the
   variable object and using property attributes { DontDelete }.

   * Eval Code
   Variable instantiation is performed using the calling context's
   variable object and using empty property attributes.
*/

var BUGNUMBER = '(none)';
var summary = '10.1.4.1 Entering An Execution Context';
var actual = '';
var expect = '';

test();

function test()
{
  enterFunc ("test");
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var y;
  eval("var x = 1");

  if (delete y)
    reportCompare('PASS', 'FAIL', "Expected *NOT* to be able to delete y");

  if (typeof x == "undefined")
    reportCompare('PASS', 'FAIL', "x did not remain defined after eval()");
  else if (x != 1)
    reportCompare('PASS', 'FAIL', "x did not retain it's value after eval()");
   
  if (!delete x)
    reportCompare('PASS', 'FAIL', "Expected to be able to delete x");

  reportCompare('PASS', 'PASS', '10.1.4.1 Entering An Execution Context');

  exitFunc("test");       
}

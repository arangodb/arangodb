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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Norris Boyd
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

var gTestfile = 'destructuring-scope.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 'none';
var summary = 'Test destructuring assignments for differing scopes';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function f() {
  var x = 3;
  if (x > 0) {
    let {a:x} = {a:7};
    if (x != 7) 
      throw "fail";
  }
  if (x != 3) 
    throw "fail";
}

function g() {
  for (var [a,b] in {x:7}) {
    if (a != "x" || b != 7) 
      throw "fail";
  }

  {
    for (let [a,b] in {y:8}) {
      if (a != "y" || b != 8) 
        throw "fail";
    }
  }

  if (a != "x" || b != 7) 
    throw "fail";
}

f();
g();

if (typeof a != "undefined" || typeof b != "undefined" || typeof x != "undefined")
  throw "fail";

function h() {
  for ([a,b] in {z:9}) {
    if (a != "z" || b != 9) 
      throw "fail";
  }
}

h();

if (a != "z" || b != 9) 
  throw "fail";


reportCompare(expect, actual, summary);


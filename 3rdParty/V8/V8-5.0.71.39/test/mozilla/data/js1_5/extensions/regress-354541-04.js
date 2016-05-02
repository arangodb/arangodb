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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Reto Laemmler
 *                 Brendan Eich
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

var gTestfile = 'regress-354541-04.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 354541;
var summary = 'Regression to standard class constructors in case labels';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary + ': in function');

  String.prototype.trim = function() { return 'hallo'; };

  const S = String;
  const Sp = String.prototype;

  expect = 'hallo';
  var expectStringInvariant = true;
  var actualStringInvariant;
  var expectStringPrototypeInvariant = true;
  var actualStringPrototypeInvariant;

  if (typeof Script == 'undefined')
  {
    print('Test skipped. Script is not defined');
    reportCompare("Script not defined, Test skipped.",
                  "Script not defined, Test skipped.",
                  summary);
  }
  else
  {
    s = Script('var tmp = function(o) { switch(o) { case String: case 1: return ""; } }; actualStringInvariant = (String === S); actualStringPrototypeInvariant = (String.prototype === Sp); actual = "".trim();');
    try
    {
      s();
    }
    catch(ex)
    {
      actual = ex + '';
    }

    reportCompare(expect, actual, 'trim() returned');
    reportCompare(expectStringInvariant, actualStringInvariant, 'String invariant');
    reportCompare(expectStringPrototypeInvariant,
                  actualStringPrototypeInvariant,
                  'String.prototype invariant');
  }
 
  exitFunc ('test');
}

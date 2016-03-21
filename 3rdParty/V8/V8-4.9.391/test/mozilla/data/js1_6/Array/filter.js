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
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

var gTestfile = 'filter.js';
//-----------------------------------------------------------------------------
var BUGNUMBER     = "364603";
var summary = "The value placed in a filtered array for an element is the " +
  " element's value before the callback is run, not after";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function mutate(val, index, arr)
{
  arr[index] = "mutated";
  return true;
}

function assertEqual(v1, v2, msg)
{
  if (v1 !== v2)
    throw msg;
}

try
{
  var a = [1, 2];
  var m = a.filter(mutate);

  assertEqual(a[0], "mutated", "Array a not mutated!");
  assertEqual(a[1], "mutated", "Array a not mutated!");

  assertEqual(m[0], 1, "Filtered value is value before callback is run");
  assertEqual(m[1], 2, "Filtered value is value before callback is run");
}
catch (e)
{
  failed = e;
}


expect = false;
actual = failed;

reportCompare(expect, actual, summary);

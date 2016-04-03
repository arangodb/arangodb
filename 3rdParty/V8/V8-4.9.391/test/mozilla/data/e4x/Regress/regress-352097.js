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
 * Contributor(s): Jesse Ruderman
 *                 Jeff Walden
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

gTestfile = 'regress-352097.js';

//-----------------------------------------------------------------------------
var BUGNUMBER     = "352097";
var summary = "Avoid adding unnecessary spaces to PIs";
var actual, expect;

printBugNumber(BUGNUMBER);
START(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function assertContains(func, str)
{
  if (func.toString().indexOf(str) < 0)
    throw func.toString() + " does not contain " + str + "!";
}

try
{
  var f = new Function("return <?f?>;");
  assertContains(f, "<?f?>");

  var g = new Function("return <?f ?>;");
  assertContains(g, "<?f?>");

  var h = new Function("return <?f       ?>;");
  assertContains(h, "<?f?>");

  var i = new Function("return <?f      i?>;");
  assertContains(i, "<?f i?>");

  var j = new Function("return <?f i ?>;");
  assertContains(j, "<?f i ?>");

  var k = new Function("return <?f i      ?>;");
  assertContains(k, "<?f i      ?>");

  var m = new Function("return <?f      i ?>;");
  assertContains(m, "<?f i ?>");
}
catch (ex)
{
  failed = ex;
}

expect = false;
actual = failed;

TEST(1, expect, actual);

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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Aleksey Chernoraenko <archer@meta-comm.com>
 *                 Brendan Eich <brendan@mozilla.org>
 *                 Bob Clary <bob@bclary.com>
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

var gTestfile = 'regress-252892.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 252892;
var summary = 'for (var i in o) in heavyweight function f should define i in f\'s activation';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var status; 

var dodis;

function f1(o){for(var x in o)printStatus(o[x]); return x}
function f2(o){with(this)for(var x in o)printStatus(o[x]); return x}
function f2novar(o){with(this)for(x in o)printStatus(o[x]); return x}
function f3(i,o){for(var x=i in o)printStatus(o[x]); return x}
function f4(i,o){with(this)for(var x=i in o)printStatus(o[x]); return x}

const f1src =
  "function f1(o) {\n" +
  "    for (var x in o) {\n" +
  "        printStatus(o[x]);\n" +
  "    }\n" +
  "    return x;\n" +
  "}";

const f2src =
  "function f2(o) {\n" +
  "    with (this) {\n" +
  "        for (var x in o) {\n" +
  "            printStatus(o[x]);\n" +
  "        }\n" +
  "    }\n" +
  "    return x;\n" +
  "}";

const f2novarsrc =
  "function f2novar(o) {\n" +
  "    with (this) {\n" +
  "        for (x in o) {\n" +
  "            printStatus(o[x]);\n" +
  "        }\n" +
  "    }\n" +
  "    return x;\n" +
  "}";

const f3src =
  "function f3(i, o) {\n" +
  "    var x = i;\n" +
  "    for (x in o) {\n" +
  "        printStatus(o[x]);\n" +
  "    }\n" +
  "    return x;\n" +
  "}";

const f4src =
  "function f4(i, o) {\n" +
  "    with (this) {\n" +
  "        var x = i;\n" +
  "        for (x in o) {\n" +
  "            printStatus(o[x]);\n" +
  "        }\n" +
  "    }\n" +
  "    return x;\n" +
  "}";

var t=0;
function assert(c)
{
  ++t;

  status = summary + ' ' + inSection(t);
  expect = true;
  actual = c;

  if (!c)
  {
    printStatus(t + " FAILED!");
  }
  reportCompare(expect, actual, summary);
}

if (dodis && this.dis) dis(f1);
assert(f1 == f1src);

if (dodis && this.dis) dis(f2);
assert(f2 == f2src);

if (dodis && this.dis) dis(f2novar);
assert(f2novar == f2novarsrc);

if (dodis && this.dis) dis(f3);
assert(f3 == f3src);

if (dodis && this.dis) dis(f4);
assert(f4 == f4src);

assert(f1([]) == undefined);

assert(f1(['first']) == 0);

assert(f2([]) == undefined);

assert(f2(['first']) == 0);

assert(f3(42, []) == 42);

assert(f3(42, ['first']) == 0);

assert(f4(42, []) == 42);

assert(f4(42, ['first']) == 0);

this.x = 41;

assert(f2([]) == undefined);

assert(f2novar([]) == 41);

assert(f2(['first']) == undefined);

assert(f2novar(['first']) == 0);

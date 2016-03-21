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
 * Contributor(s): Brendan Eich
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

var gTestfile = 'regress-350312-03.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 350312;
var summary = 'Accessing wrong stack slot with nested catch/finally';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var pfx  = "(function (x) {try {if (x > 41) throw x}",
    cg1a = " catch (e if e === 42) {var v = 'catch guard 1 ' + e; actual += v + ',';print(v);}"
    cg1b = " catch (e if e === 42) {var v = 'catch guard 1 + throw ' + e; actual += v + ',';print(v); throw e;}"
    cg2  = " catch (e if e === 43) {var v = 'catch guard 2 ' + e; actual += v + ',';print(v)}"
    cat  = " catch (e) {var v = 'catch all ' + e; print(v); if (e == 44) throw e}"
    fin  = " finally{var v = 'fin'; actual += v + ',';print(v)}",
    end  = "})";

  var exphash  = {
    pfx: "(function (y) { var result = ''; y = y + ',';",
    cg1a: " result += (y === '42,') ? ('catch guard 1 ' + y):'';",
    cg1b: " result += (y === '42,') ? ('catch guard 1 + throw ' + y):'';",
    cg2:  " result += (y === '43,') ? ('catch guard 2 ' + y):'';",
    cat:  " result += (y > 41) ? ('catch all ' + y):'';",
    fin:  " result += 'fin,';",
    end:  "return result;})"
  };

  var src = [
    pfx + fin + end,
    pfx + cat + end,
    pfx + cat + fin + end,
    pfx + cg1a + end,
    pfx + cg1a + fin + end,
    pfx + cg1a + cat + end,
    pfx + cg1a + cat + fin + end,
    pfx + cg1a + cg2 + end,
    pfx + cg1a + cg2 + fin + end,
    pfx + cg1a + cg2 + cat + end,
    pfx + cg1a + cg2 + cat + fin + end,
    pfx + cg1b + end,
    pfx + cg1b + fin + end,
    pfx + cg1b + cat + end,
    pfx + cg1b + cat + fin + end,
    pfx + cg1b + cg2 + end,
    pfx + cg1b + cg2 + fin + end,
    pfx + cg1b + cg2 + cat + end,
    pfx + cg1b + cg2 + cat + fin + end,
    ];

  var expsrc = [
    exphash.pfx + exphash.fin + exphash.end,
    exphash.pfx + exphash.cat + exphash.end,
    exphash.pfx + exphash.cat + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.cat + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.cat + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.cg2 + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.cg2 + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.cg2 + exphash.cat + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.cg2 + exphash.cat + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.cat + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.cat + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.cg2 + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.cg2 + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.cg2 + exphash.cat + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.cg2 + exphash.cat + exphash.fin + exphash.end,
    ];

  for (var i in src) {
    print("\n=== " + i + ": " + src[i]);
    var f = eval(src[i]);
    var exp = eval(expsrc[i]);
    // dis(f);
    print('decompiling: ' + f);
    //print('decompiling exp: ' + exp);

    actual = '';
    try { expect = exp(41); f(41) } catch (e) { print('tried f(41), caught ' + e) }
    reportCompare(expect, actual, summary);

    actual = '';
    try { expect = exp(42); f(42) } catch (e) { print('tried f(42), caught ' + e) }
    reportCompare(expect, actual, summary);

    actual = '';
    try { expect = exp(43); f(43) } catch (e) { print('tried f(43), caught ' + e) }
    reportCompare(expect, actual, summary);

    actual = '';
    try { expect = exp(44); f(44) } catch (e) { print('tried f(44), caught ' + e) }
    reportCompare(expect, actual, summary);

    actual = '';
    try { expect = exp(45); f(45) } catch (e) { print('tried f(44), caught ' + e) }
    reportCompare(expect, actual, summary);

  }

  exitFunc ('test');
}

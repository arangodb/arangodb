/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

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
 * Contributor(s): Igor Bukanov
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

gTestfile = 'regress-339785.js';

var summary = "scanner: memory exposure to scripts";
var BUGNUMBER = 339785;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

function evalXML(N)
{
    var str = Array(N + 1).join('a'); // str is string of N a
    src = "var x = <xml>&"+str+";</xml>;";
    try {
        eval(src);
        return "Should have thrown unknown entity error";
    } catch (e) {
        return e.message;
    }
    return "Unexpected";
}

var N1 = 1;
var must_be_good = evalXML(N1);
expect = 'unknown XML entity a';
actual = must_be_good;
TEST(1, expect, actual);

function testScanner()
{
    for (var power = 2; power != 15; ++power) {
        var N2 = (1 << power) - 2;
        var can_be_bad  = evalXML(N2);
        var diff = can_be_bad.length - must_be_good.length;
        if (diff != 0 && diff != N2 - N1) {
            return "Detected memory exposure at entity length of "+(N2+2);
        }
    }
    return "Ok";
}

expect = "Ok";

// repeat test since test does not always fail

for (var iTestScanner = 0; iTestScanner < 100; ++iTestScanner)
{
    actual = testScanner();
    TEST(iTestScanner+1, expect, actual);
}


END();

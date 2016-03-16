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

gTestfile = '11.4.7-02.js';

/**
 *  File Name:          11.4.7-02.js
 *  Reference:          https://bugzilla.mozilla.org/show_bug.cgi?id=432881
 *  Description:        ecma 11.4.7
 */

var SECTION = "11.4.7";
var VERSION = "ECMA";
var TITLE   = "Unary - Operator";
var BUGNUMBER = "432881";

startTest();

test_negation(0, -0.0);
test_negation(-0.0, 0);
test_negation(1, -1);
test_negation(1.0/0.0, -1.0/0.0);
test_negation(-1.0/0.0, 1.0/0.0);

//1073741824 == (1 << 30)
test_negation(1073741824, -1073741824);
test_negation(-1073741824, 1073741824);

//1073741824 == (1 << 30) - 1
test_negation(1073741823, -1073741823);
test_negation(-1073741823, 1073741823);

//1073741824 == (1 << 30)
test_negation(1073741824, -1073741824);
test_negation(-1073741824, 1073741824);

//1073741824 == (1 << 30) - 1
test_negation(1073741823, -1073741823);
test_negation(-1073741823, 1073741823);

//2147483648 == (1 << 31)
test_negation(2147483648, -2147483648);
test_negation(-2147483648, 2147483648);

//2147483648 == (1 << 31) - 1
test_negation(2147483647, -2147483647);
test_negation(-2147483647, 2147483647);

function test_negation(value, expected)
{
    var actual = -value;
    reportCompare(expected, actual, '-(' + value + ') == ' + expected);
} 

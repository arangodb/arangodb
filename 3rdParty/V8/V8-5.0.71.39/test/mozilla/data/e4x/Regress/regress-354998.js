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

gTestfile = 'regress-354998.js';

var BUGNUMBER = 354998;
var summary = 'prototype should not be enumerated for XML objects.';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

function test()
{
    var list = <><a/><b/></>;
    var count = 0;
    var now = Date.now;
    var time = now();
    for (var i in list) {
        ++count;
    }
    time = now() - time;
    if (count != 2) {
        if (count < 2)
            throw "Enumerator has not looped over all properties, count="+count;
        throw "Enumerator has looped over prototype chain of xml, count="+count;
    }
    return time;
}

try
{
    var time1 = test();

    for (var i = 0; i != 1000*1000; ++i)
        Object.prototype[i] = i;

    var time2 = test();

    // clean up Object prototype so it won't
    // hang enumerations in options()...

    for (var i = 0; i != 1000*1000; ++i)
        delete Object.prototype[i];

    if (time1 * 10 + 1 < time2) {
        throw "Assigns to Object.prototype increased time of XML enumeration from "+
            time1+"ms to "+time2+"ms";
    }
}
catch(ex)
{
    actual = ex = '';
}

TEST(1, expect, actual);

END();

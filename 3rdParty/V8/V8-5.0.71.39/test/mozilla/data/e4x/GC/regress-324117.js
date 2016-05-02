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

gTestfile = 'regress-324117.js';

var summary = "GC hazard during namespace scanning";
var BUGNUMBER = 324117;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

function prepare(N)
{
    var xml = <xml/>;
    var ns1 = new Namespace("text1"); 
    var ns2 = new Namespace("text2"); 
    xml.addNamespace(ns1);
    xml.addNamespace(ns2);

    // Prepare list to trigger DeutschSchorrWaite call during GC
    cursor = xml;
    for (var i = 0; i != N; ++i) {
        if (i % 2 == 0)
            cursor = [ {a: 1}, cursor ];
        else
            cursor = [ cursor, {a: 1} ];
    }
    return cursor;
}

function check(list, N)
{
    // Extract xml to verify
    for (var i = N; i != 0; --i) {
        list = list[i % 2];
    }
    var xml = list;
    if (typeof xml != "xml")
        return false;
    var array = xml.inScopeNamespaces();
    if (array.length !== 3)
        return false;
    if (array[0].uri !== "")
        return false;
    if (array[1].uri !== "text1")
        return false;
    if (array[2].uri !== "text2")
        return false;

    return true;
}

var N = 64000;
var list = prepare(N);
gc();
var ok = check(list, N);
printStatus(ok);

TEST(1, expect, actual);

END();

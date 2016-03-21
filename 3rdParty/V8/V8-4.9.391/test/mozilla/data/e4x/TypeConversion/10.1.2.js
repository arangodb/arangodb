/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
 *   Ethan Hugg
 *   Milen Nankov
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

gTestfile = '10.1.2.js';

START("10.1.2 - XMLList.toString");

var n = 0;
var expect;
var actual;
var xmllist;
var xml;

// Example from ECMA 357 10.1.1

var order =
<order>
    <customer>
        <firstname>John</firstname>
        <lastname>Doe</lastname>
    </customer>
    <item>
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
</order>;

// Semantics

printStatus("test empty.toString()");

xmllist = new XMLList();
expect = '';
actual = xmllist.toString();
TEST(++n, expect, actual);

printStatus("test [attribute].toString()");

xmllist = new XMLList();
xml = <foo bar="baz"/>;
var attr = xml.@bar;
xmllist[0] = attr;
expect = "baz";
actual = xmllist.toString();
TEST(++n, expect, actual);

printStatus("test [text].toString()");

xmllist = new XMLList();
xml = new XML("this is text");
xmllist[0] = xml;
expect = "this is text";
actual = xmllist.toString();
TEST(++n, expect, actual);

printStatus("test [simpleContent].toString()");

xmllist = new XMLList();
xml = <foo>bar</foo>;
xmllist[0] = xml;
expect = "bar";
actual = xmllist.toString();
TEST(++n, expect, actual);

var truefalse = [true, false];

for (var ic = 0; ic < truefalse.length; ic++)
{
    for (var ip = 0; ip < truefalse.length; ip++)
    {
        XML.ignoreComments = truefalse[ic];
        XML.ignoreProcessingInstructions = truefalse[ip];

        xmllist = new XMLList();
        xml = <foo><!-- comment1 --><?pi1 ?>ba<!-- comment2 -->r<?pi2 ?></foo>;
        xmllist[0] = xml;
        expect = "bar";
        actual = xmllist.toString();
        TEST(++n, expect, actual);
    }
}

printStatus("test nonSimpleContent.toString() == " +
            "nonSimpleContent.toXMLString()");

var indents = [0, 4];

for (var pp = 0; pp < truefalse.length; pp++)
{
    XML.prettyPrinting = truefalse[pp];
    for (var pi = 0; pi < indents.length; pi++)
    {
        XML.prettyIndent = indents[pi];
  
        xmllist = new XMLList(order);
        expect = order.toXMLString();
        actual = order.toString();
        TEST(++n, expect, actual);

        xmllist = order..*;
        expect = order.toXMLString();
        actual = order.toString();
        TEST(++n, expect, actual);

    }
}

END();

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

gTestfile = '13.5.4.5.js';

START("13.5.4.5 - XMLList children()");

TEST(1, true, XMLList.prototype.hasOwnProperty("children"));

// Test with XMLList of size 0
x = new XMLList()
TEST(2, "xml", typeof(x.children()));
TEST_XML(3, "", x.children());

// Test with XMLList of size 1
x += <alpha>one<bravo>two</bravo></alpha>;   
TEST(4, "xml", typeof(x.children()));

correct = <>one<bravo>two</bravo></>;
TEST(5, correct, x.children());

// Test with XMLList of size > 1
x += <charlie><bravo>three</bravo></charlie>;
TEST(6, "xml", typeof(x.children()));

correct = <>one<bravo>two</bravo><bravo>three</bravo></>;
TEST(7, correct, x.children());

// Test no children
x = new XMLList();
x += <alpha></alpha>;
x += <bravo></bravo>;
TEST(8, "xml", typeof(x.children()));
TEST_XML(9, "", x.children());

//get all grandchildren of the order that have the name price

order =
<order>
    <customer>
        <name>John Smith</name>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
</order>;

correct= <price>1299.99</price> + <price>399.99</price>;

TEST(10, correct, order.children().price);

END();

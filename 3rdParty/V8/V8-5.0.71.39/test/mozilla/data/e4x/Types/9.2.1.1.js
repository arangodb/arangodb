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

gTestfile = '9.2.1.1.js';

START("9.2.1.1 XMLList [[Get]]");

var x =
<>
<alpha attr1="value1">
    <bravo attr2="value2">
        one
        <charlie>two</charlie>
    </bravo>
</alpha>
<alpha attr1="value3">
    <bravo attr2="value4">
        three
        <charlie>four</charlie>
    </bravo>
</alpha>
</>;

// .
correct =
<>
    <bravo attr2="value2">
        one
        <charlie>two</charlie>
    </bravo>
    <bravo attr2="value4">
        three
        <charlie>four</charlie>
    </bravo>
</>;

TEST(1, correct, x.bravo);

correct =
<>
    <charlie>two</charlie>
    <charlie>four</charlie>
</>;

TEST(2, correct, x.bravo.charlie);

// .@
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value3");
TEST(3, correct, x.@attr1);

correct = new XMLList();
correct += new XML("value2");
correct += new XML("value4");
TEST(4, correct, x.bravo.@attr2);

// ..
correct =
<>
    <bravo attr2="value2">
        one
        <charlie>two</charlie>
    </bravo>
    <bravo attr2="value4">
        three
        <charlie>four</charlie>
    </bravo>
</>;

TEST(5, correct, x..bravo);

correct =
<>
    <charlie>two</charlie>
    <charlie>four</charlie>
</>;

TEST(6, correct, x..charlie);

// .@*
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value3");
TEST(7, correct, x.@*);

x =
<alpha attr1="value1" attr2="value2">
    <bravo>
        one
        <charlie>two</charlie>
    </bravo>
</alpha>;

// ..*
correct = <><bravo>one<charlie>two</charlie></bravo>one<charlie>two</charlie>two</>;

XML.prettyPrinting = false;
TEST(8, correct, x..*);
XML.prettyPrinting = true;

x =
<alpha attr1="value1" attr2="value2">
    <bravo attr2="value3">
        one
        <charlie attr3="value4">two</charlie>
    </bravo>
</alpha>;

// ..@
correct = new XMLList();
correct += new XML("value2");
correct += new XML("value3");
TEST(9, correct, x..@attr2)

// ..@*
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value2");
correct += new XML("value3");
correct += new XML("value4");
TEST(10, correct, x..@*);


END();
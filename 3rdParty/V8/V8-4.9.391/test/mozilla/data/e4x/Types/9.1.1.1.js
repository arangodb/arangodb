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

gTestfile = '9.1.1.1.js';

START('9.1.1.1 XML [[Get]]');

var x =
<alpha attr1="value1" attr2="value2">
    <bravo>
        one
        <charlie>two</charlie>
    </bravo>
</alpha>;

// .
TEST(1, <bravo>one<charlie>two</charlie></bravo>, x.bravo);
TEST(2, <charlie>two</charlie>, x.bravo.charlie);
TEST(3, <charlie>two</charlie>, x.bravo.charlie.parent().charlie);

// .*
var correct = <>one<charlie>two</charlie></>;
TEST(4, correct, x.bravo.*);
TEST_XML(5, "two", x.bravo.charlie.*);
TEST(6, <bravo>one<charlie>two</charlie></bravo>, x.*[0]);

// .@
TEST_XML(7, "value1", x.@attr1);
TEST_XML(8, "value2", x.@attr2);

// ..
TEST(9, <bravo>one<charlie>two</charlie></bravo>, x..bravo);
TEST(10, <charlie>two</charlie>, x..charlie);

// .@*
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value2");
TEST(11, correct, x.@*);

x =
<alpha attr1="value1" attr2="value2">
    <bravo>
        one
        <charlie>two</charlie>
    </bravo>
</alpha>;

// ..*
XML.prettyPrinting = false;
correct = <><bravo>one<charlie>two</charlie></bravo>one<charlie>two</charlie>two</>;
TEST(12, correct, x..*);
XML.prettyPrinting = true;

x =
<alpha attr1="value1" attr2="value2">
    <bravo attr3="value3">
        one
        <charlie attr3="value4">two</charlie>
    </bravo>
</alpha>;

// ..@
correct = new XMLList();
correct += new XML("value3");
correct += new XML("value4");
TEST(13, correct, x..@attr3)

// ..@*
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value2");
correct += new XML("value3");
correct += new XML("value4");
TEST(14, correct, x..@*);

// Check reserved words
x =
<alpha>
    <prototype>one</prototype>
</alpha>;

TEST(15, <prototype>one</prototype>, x.prototype);

// Check method names
x =
<alpha>
    <name>one</name>
    <toString>two</toString>
</alpha>;

TEST(16, <name>one</name>, x.name);
TEST(17, QName("alpha"), x.name());
TEST(18, <toString>two</toString>, x.toString);
TEST(19, x.toXMLString(), x.toString());

// Test super-expandos
x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        one
        <charlie>
            <delta>two</delta>
        </charlie>
     </bravo>
</alpha>;

x.bravo.charlie.delta = <delta>two</delta>;
TEST(20, correct, x);

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        one
        <charlie>
            <delta>two</delta>
        </charlie>
     </bravo>
</alpha>;

x.bravo.charlie.delta = "two";
TEST(21, correct, x);

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        one
        <charlie>
            <echo>two</echo>
        </charlie>
     </bravo>
</alpha>;

x.bravo.charlie.delta = <echo>two</echo>;
TEST(22, correct, x);

// Also ADD with *, children() and child()
x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        one
        <charlie>
            <delta>two</delta>
        </charlie>
     </bravo>
</alpha>;

x.*.charlie.delta = <delta>two</delta>;
TEST(23, correct, x);

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        one
        <charlie>
            <delta>two</delta>
        </charlie>
     </bravo>
</alpha>;

x.children().charlie.delta = <delta>two</delta>;
TEST(24, correct, x);

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        one
        <charlie>
            <delta>two</delta>
        </charlie>
     </bravo>
</alpha>;

x.child("bravo").charlie.delta = <delta>two</delta>;
TEST(25, correct, x);

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>one</bravo>
    <newChild>
        <charlie>
            <delta>two</delta>
        </charlie>
     </newChild>
</alpha>;

x.child("newChild").charlie.delta = <delta>two</delta>;
TEST(26, correct, x);

// These should fail because the XMLList is size > 1
x =
<alpha>
    <bravo>one</bravo>
    <bravo>two</bravo>
</alpha>;

try {
    x.*.charlie.delta = "more";
    SHOULD_THROW(27);
} catch (ex) {
    TEST(27, "TypeError", ex.name);
}

x =
<alpha>
    <bravo>one</bravo>
    <bravo>two</bravo>
</alpha>;

try {
    x.children().charlie.delta = "more";
    SHOULD_THROW(28);
} catch (ex) {
    TEST(28, "TypeError", ex.name);
}

x =
<alpha>
    <bravo>one</bravo>
    <bravo>two</bravo>
</alpha>;

try {
    x.child("bravo").charlie.delta = "more";
    SHOULD_THROW(29);
} catch (ex) {
    TEST(29, "TypeError", ex.name);
}

END();
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

gTestfile = '13.4.4.39.js';

START("13.4.4.39 - XML toXMLString");

TEST(1, true, XML.prototype.hasOwnProperty("toXMLString"));

XML.prettyPrinting = false;

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(2, "<bravo>one</bravo>", x.bravo.toXMLString());
TEST(3, "<bravo>one</bravo><bravo>two</bravo>", x..bravo.toXMLString());

x =
<alpha>
    <bravo>one</bravo>
    <charlie/>
</alpha>;

TEST(4, "<charlie/>", x.charlie.toXMLString());

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(5, "<charlie><bravo>two</bravo></charlie>", x.charlie.toXMLString());

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        two
        <bravo/>
    </charlie>
</alpha>;

TEST(6, "<charlie>two<bravo/></charlie>", x.charlie.toXMLString());

x =
<alpha>
    <bravo></bravo>
    <bravo/>
</alpha>;

TEST(7, "<bravo/><bravo/>", x.bravo.toXMLString());

x =
<alpha>
    <bravo>one<charlie/></bravo>
    <bravo>two<charlie/></bravo>
</alpha>;

TEST(8, "<bravo>one<charlie/></bravo><bravo>two<charlie/></bravo>", x.bravo.toXMLString());
  
XML.prettyPrinting = true;

x =
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
</alpha>;

copy = x.bravo.copy();

TEST(9, "<bravo>one</bravo>", copy.toXMLString());

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(10, "String contains value one from bravo", "String contains value " + x.bravo + " from bravo");

END();

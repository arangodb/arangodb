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

gTestfile = '13.5.2.js';

START("13.5.2 - XMLList Constructor");

x = new XMLList();
TEST(1, "xml", typeof(x));  
TEST(2, true, x instanceof XMLList);

// Load from another XMLList
// Make sure it is copied if it's an XMLList
x =
<>
    <alpha>one</alpha>
    <bravo>two</bravo>
</>;

y = new XMLList(x);

x += <charlie>three</charlie>;

TEST(3, "<alpha>one</alpha>\n<bravo>two</bravo>", y.toString());
  
// Load from one XML type
x = new XMLList(<alpha>one</alpha>);
TEST_XML(4, "<alpha>one</alpha>", x);

// Load from Anonymous
x = new XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
TEST(5, "<alpha>one</alpha>\n<bravo>two</bravo>", x.toString());

// Load from Anonymous as string
x = new XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
TEST(6, "<alpha>one</alpha>\n<bravo>two</bravo>", x.toString());

// Load from single textnode
x = new XMLList("foobar");
TEST_XML(7, "foobar", x);

x = XMLList(7);
TEST_XML(8, 7, x);

// Undefined and null should behave like ""
x = new XMLList(null);
TEST_XML(9, "", x);

x = new XMLList(undefined);
TEST_XML(10, "", x);

END();
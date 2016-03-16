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

gTestfile = '11.2.2.js';

START("11.2.2 - Function Calls");


rectangle = <rectangle>
            <x>50</x>
            <y>75</y>
            <length>20</length>
            <width>30</width>
            </rectangle>;   


TEST(1, 1, rectangle.length());

TEST(2, <length>20</length>, rectangle.length);

shipto = <shipto>
         <name>Fred Jones</name>
         <street>123 Foobar Ave.</street>
         <citystatezip>Redmond, WA, 98008</citystatezip>
         </shipto>;


upperName = shipto.name.toUpperCase();
TEST(3, "FRED JONES", upperName);

upperName = shipto.name.toString().toUpperCase();
TEST(4, "FRED JONES", upperName);
upperName = shipto.name.toUpperCase();
TEST(5, "FRED JONES", upperName);

citystatezip = shipto.citystatezip.split(", ");
state = citystatezip[1];
TEST(6, "WA", state);
zip = citystatezip[2];
TEST(7, "98008", zip);

citystatezip = shipto.citystatezip.toString().split(", ");
state = citystatezip[1];
TEST(8, "WA", state);
zip = citystatezip[2];
TEST(9, "98008", zip);

// Test method name/element name conflicts

x =
<alpha>
    <name>Foo</name>
    <length>Bar</length>
</alpha>;

TEST(10, <name>Foo</name>, x.name);
TEST(11, QName("alpha"), x.name());
TEST(12, <length>Bar</length>, x.length);
TEST(13, 1, x.length());
TEST(14, x, x.(name == "Foo"));
x.name = "foobar";
TEST(15, <name>foobar</name>, x.name);
TEST(16, QName("alpha"), x.name());



END();

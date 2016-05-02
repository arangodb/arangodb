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

gTestfile = '10.4.js';

START("10.4 - toXMLList");

var x;

// null
try {
    x = null;
    x.toString();
    SHOULD_THROW(1);
} catch (ex) {
    TEST(1, "TypeError", ex.name);
}

// number
x = new Number(123);
TEST(2, "123", new XMLList(x).toXMLString());

// String
x = new String("<alpha><bravo>one</bravo></alpha>");
TEST(3, <alpha><bravo>one</bravo></alpha>, new XMLList(x));

x = new String("<alpha>one</alpha><charlie>two</charlie>");
TEST(4, "<alpha>one</alpha>\n<charlie>two</charlie>",
  new XMLList(x).toXMLString());

// XML
x = new XML(<alpha><bravo>one</bravo></alpha>);
TEST(5, <alpha><bravo>one</bravo></alpha>, new XMLList(x));

x = new XML(<root><alpha><bravo>one</bravo></alpha><charlie>two</charlie></root>);
TEST(6, <alpha><bravo>one</bravo></alpha>, new XMLList(x.alpha));

// XMLList
x = new XMLList(<alpha><bravo>one</bravo></alpha>);
TEST(7, <alpha><bravo>one</bravo></alpha>, new XMLList(x));

x = new XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
TEST(8, "<alpha>one</alpha>\n<bravo>two</bravo>",
  new XMLList(x).toXMLString());
   
END();
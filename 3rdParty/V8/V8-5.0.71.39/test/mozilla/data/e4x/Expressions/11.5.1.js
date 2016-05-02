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

gTestfile = '11.5.1.js';

START("11.5.1 - Equality Operators");

x = <alpha>one</alpha>;
y = <alpha>one</alpha>;
TEST(1, true, (x == y) && (y == x));

// Should return false if comparison is not XML
y = "<alpha>one</alpha>";
TEST(2, false, (x == y) || (y == x));

y = undefined
TEST(3, false, (x == y) || (y == x));

y = null
TEST(4, false, (x == y) || (y == x));

// Should check logical equiv.
x = <alpha attr1="value1">one<bravo attr2="value2">two</bravo></alpha>;
y = <alpha attr1="value1">one<bravo attr2="value2">two</bravo></alpha>;
TEST(5, true, (x == y) && (y == x));

y = <alpha attr1="new value">one<bravo attr2="value2">two</bravo></alpha>;
TEST(6, false, (x == y) || (y == x));

m = new Namespace();
n = new Namespace();
TEST(7, true, m == n);

m = new Namespace("uri");
TEST(8, false, m == n);

n = new Namespace("ns", "uri");
TEST(9, true, m == n);

m = new Namespace(n);
TEST(10, true, m == n);

TEST(11, false, m == null);
TEST(12, false, null == m);

m = new Namespace("ns", "http://anotheruri");
TEST(13, false, m == n);

p = new QName("a");
q = new QName("b");
TEST(14, false, p == q);

q = new QName("a");
TEST(15, true, p == q);

q = new QName("http://someuri", "a");
TEST(16, false, p == q);

q = new QName(null, "a");
TEST(16, false, p == q);

END();
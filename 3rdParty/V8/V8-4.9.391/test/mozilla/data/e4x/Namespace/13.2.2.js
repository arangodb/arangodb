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

gTestfile = '13.2.2.js';

START("13.2.2 - Namespace Constructor");

n = new Namespace();
TEST(1, "object", typeof(n));
TEST(2, "", n.prefix);
TEST(3, "", n.uri);

n = new Namespace("");
TEST(4, "object", typeof(n));
TEST(5, "", n.prefix);
TEST(6, "", n.uri);

n = new Namespace("http://foobar/");
TEST(7, "object", typeof(n));
TEST(8, "undefined", typeof(n.prefix));
TEST(9, "http://foobar/", n.uri);

// Check if the undefined prefix is getting set properly
m = new Namespace(n);
TEST(10, typeof(n), typeof(m));
TEST(11, n.prefix, m.prefix);
TEST(12, n.uri, m.uri);

n = new Namespace("foobar", "http://foobar/");
TEST(13, "object", typeof(n));
TEST(14, "foobar", n.prefix);
TEST(15, "http://foobar/", n.uri);

// Check if all the properties are getting copied
m = new Namespace(n);
TEST(16, typeof(n), typeof(m));
TEST(17, n.prefix, m.prefix);
TEST(18, n.uri, m.uri);

try {
    n = new Namespace("ns", "");
    SHOULD_THROW(19);
} catch(ex) {
    TEST(19, "TypeError", ex.name);
}

END();
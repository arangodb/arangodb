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
 *   Jeff Walden <jwalden+code@mit.edu>
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

gTestfile = '13.4.4.26.js';

START("13.4.4.26 - XML normalize()");

TEST(1, true, XML.prototype.hasOwnProperty("normalize"));

XML.ignoreWhitespace = false;
XML.prettyPrinting = false;

var x = <alpha> <bravo> one </bravo> </alpha>;

TEST_XML(2, "<alpha> <bravo> one </bravo> </alpha>", x);
x.normalize();
TEST_XML(3, "<alpha> <bravo> one </bravo> </alpha>", x);


// First, test text node coalescing

delete x.bravo[0];

TEST_XML(4, "<alpha>  </alpha>", x);
TEST(5, 2, x.children().length());

x.normalize();

TEST_XML(6, "<alpha>  </alpha>", x);
TEST(7, 1, x.children().length());

// check that nodes are inserted in the right place after a normalization

x.appendChild(<bravo> fun </bravo>);

TEST_XML(8, "<alpha>  <bravo> fun </bravo></alpha>", x);
TEST(9, 2, x.children().length());

// recursive nature

var y = <charlie> <delta/> </charlie>;

TEST(10, 3, y.children().length());

x.appendChild(y);
delete y.delta[0];

TEST(11, 2, y.children().length());

x.normalize();

TEST(12, 1, y.children().length());
TEST(13, 1, x.charlie.children().length());



// Second, test empty text node removal

x = <alpha><beta/></alpha>;

TEST_XML(14, "<alpha><beta/></alpha>", x);
TEST(15, 1, x.children().length());

x.appendChild(XML());

TEST_XML(16, "<alpha><beta/></alpha>", x);
TEST(17, 2, x.children().length());

x.normalize();

TEST_XML(18, "<alpha><beta/></alpha>", x);
TEST(19, 1, x.children().length());

x.appendChild(XML(" "));

TEST_XML(20, "<alpha><beta/> </alpha>", x);
TEST(21, 2, x.children().length());

x.normalize();

// normalize does not remove whitespace-only text nodes
TEST_XML(22, "<alpha><beta/> </alpha>", x);
TEST(23, 2, x.children().length());

y = <foo/>;
y.appendChild(XML());

TEST(24, 1, y.children().length());

x.appendChild(y);

// check recursive nature

x.normalize();

TEST(25, 0, y.children().length());

END();

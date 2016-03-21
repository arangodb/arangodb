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

gTestfile = '13.4.4.35.js';

START("13.4.4.35 - setName");

TEST(1, true, XML.prototype.hasOwnProperty("setName"));

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<charlie>
    <bravo>one</bravo>
</charlie>;

x.setName("charlie");

TEST(2, correct, x);

x =
<ns:alpha xmlns:ns="http://foobar/">
    <ns:bravo>one</ns:bravo>
</ns:alpha>;

correct =
<charlie xmlns:ns="http://foobar/">
    <ns:bravo>one</ns:bravo>
</charlie>;

x.setName("charlie");

TEST(3, correct, x);

x =
<ns:alpha xmlns:ns="http://foobar/">
    <ns:bravo>one</ns:bravo>
</ns:alpha>;

correct =
<ns:charlie xmlns:ns="http://foobar/">
    <ns:bravo>one</ns:bravo>
</ns:charlie>;

x.setName(new QName("http://foobar/", "charlie"));

TEST(4, correct, x);

END();
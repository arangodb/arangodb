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
 *   Ethan Hugg
 *   Milen Nankov
 *   Seno Aiko
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

gTestfile = '9.1.1.3.js';

START("9.1.1.3 - XML [[Delete]]");

// .@
x =
<alpha attr1="value1">one</alpha>;

delete x.@attr1;
TEST_XML(1, "", x.@attr1);
TEST(2, <alpha>one</alpha>, x);

// ..@
x =
<alpha attr1="value1">
    one
    <bravo attr2="value2">
        two
        <charlie attr3="value3">three</charlie>
    </bravo>
</alpha>;

correct =
<alpha attr1="value1">
    one
    <bravo attr2="value2">
        two
        <charlie>three</charlie>
    </bravo>
</alpha>;

delete x..@attr3;
TEST(3, correct, x);

// ..@*
x =
<alpha attr1="value1">
    one
    <bravo attr2="value2">
        two
        <charlie attr3="value3">three</charlie>
    </bravo>
</alpha>;

correct =
<alpha>
    one
    <bravo>
        two
        <charlie>three</charlie>
    </bravo>
</alpha>;

delete x..@*;
TEST(4, correct, x);

x =
<BODY>
    <HR id="i1"/>
    <HR id="i2"/>
    <HR id="i3"/>
    <HR id="i4"/>
</BODY>;

correct =
<BODY></BODY>;

delete x..HR;
TEST(5, correct, x);

x =
<BODY>
    ECMA-357
    <HR id="i1"/>
    <HR id="i2"/>
    <HR id="i3"/>
    <HR id="i4"/>
</BODY>;

correct =
<BODY>
    ECMA-357
</BODY>;

delete x..HR;
TEST(6, correct.toXMLString(), x.toXMLString());

x =
<BODY>
    ECMA-357
    <HR id="i1"/>
    <HR id="i2"/>
    <HR id="i3"/>
    <HR id="i4"/>
</BODY>;

correct =
<BODY>ECMA-357</BODY>;

delete x.HR;
TEST(7, correct, x);

x =
<BODY>
    ECMA-357
    <HR id="i1"/>
    <HR id="i2"/>
    <HR id="i3"/>
    <HR id="i4"/>
</BODY>;

correct =
<BODY></BODY>;

delete x..*;
TEST(8, correct, x);

x =
<BODY>
    ECMA-357
    <HR id="i1"/>
    <HR id="i2"/>
    <HR id="i3"/>
    <HR id="i4"/>
</BODY>;

correct =
<BODY></BODY>;

delete x.*;
TEST(9, correct, x);

x =
<BODY>
    <UL>
      <LI id="i1"/>
      <LI id="i2"/>
      <LI id="i3"/>
    </UL>
</BODY>;
     
correct =
<BODY>
    <UL>
      <LI id="i1"/>
      <LI id="i2"/>
      <LI id="i3"/>
    </UL>
</BODY>;

delete x.LI;
TEST(10, correct, x);

x =
<BODY>
    <UL>
      <LI id="i1"/>
      <LI id="i2"/>
      <LI id="i3"/>
    </UL>
</BODY>;
     
correct =
<BODY>
    <UL></UL>
</BODY>;

delete x..LI;
TEST(11, correct, x);

END();

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

gTestfile = '13.4.4.28.js';

START("13.4.4.28 - processingInsructions()");

TEST(1, true, XML.prototype.hasOwnProperty("processingInstructions"));

XML.ignoreProcessingInstructions = false;

// test generic PI
x = <alpha><?xyz abc="123" michael="wierd"?><?another name="value" ?><bravo>one</bravo></alpha>;

correct = <><?xyz abc="123" michael="wierd"?><?another name="value" ?></>;

TEST(2, correct, x.processingInstructions());
TEST(3, correct, x.processingInstructions("*"));

correct = "<?xyz abc=\"123\" michael=\"wierd\"?>";

TEST_XML(4, correct, x.processingInstructions("xyz"));

// test XML-decl
// Un-comment these tests when we can read in doc starting with PI.
//x = new XML("<?xml version=\"1.0\" ?><alpha><bravo>one</bravo></alpha>");

//correct = new XMLList(<?xml version="1.0" encoding="utf-8"?>);

//test(5, correct, x.processingInstructions());
//test(6, correct, x.processingInstructions("*"));
//test(7, correct, x.processingInstructions("xml"));

END();

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

gTestfile = '7.7.3-2.js';

/**
   File Name:          7.7.3-2.js
   ECMA Section:       7.7.3 Numeric Literals

   Description:

   This is a regression test for
   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=122884

   Waldemar's comments:

   A numeric literal that starts with either '08' or '09' is interpreted as a
   decimal literal; it should be an error instead.  (Strictly speaking, according
   to ECMA v1 such literals should be interpreted as two integers -- a zero
   followed by a decimal number whose first digit is 8 or 9, but this is a bug in
   ECMA that will be fixed in v2.  In any case, there is no place in the grammar
   where two consecutive numbers would be legal.)

   Author:             christine@netscape.com
   Date:               15 june 1998

*/
var SECTION = "7.7.3-2";
var VERSION = "ECMA_1";
var TITLE   = "Numeric Literals";
var BUGNUMBER="122884";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "9",
	      9,
	      9 );

new TestCase( SECTION,
	      "09",
	      9,
	      09 );

new TestCase( SECTION,
	      "099",
	      99,
	      099 );


new TestCase( SECTION,
	      "077",
	      63,
	      077 );

test();

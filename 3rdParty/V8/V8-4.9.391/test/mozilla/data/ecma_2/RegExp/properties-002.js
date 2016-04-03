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
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communication Corporation.
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

gTestfile = 'properties-002.js';

/**
 *  File Name:          RegExp/properties-002.js
 *  ECMA Section:       15.7.6.js
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */
//-----------------------------------------------------------------------------
var SECTION = "RegExp/properties-002.js";
var VERSION = "ECMA_2";
var TITLE   = "Properties of RegExp Instances";
var BUGNUMBER ="124339";

startTest();

re_1 = /\cA?/g;
re_1.lastIndex = Math.pow(2,31);
AddRegExpCases( re_1, "\\cA?", true, false, false, Math.pow(2,31) );

re_2 = /\w*/i;
re_2.lastIndex = Math.pow(2,32) -1;
AddRegExpCases( re_2, "\\w*", false, true, false, Math.pow(2,32)-1 );

re_3 = /\*{0,80}/m;
re_3.lastIndex = Math.pow(2,31) -1;
AddRegExpCases( re_3, "\\*{0,80}", false, false, true, Math.pow(2,31) -1 );

re_4 = /^./gim;
re_4.lastIndex = Math.pow(2,30) -1;
AddRegExpCases( re_4, "^.", true, true, true, Math.pow(2,30) -1 );

re_5 = /\B/;
re_5.lastIndex = Math.pow(2,30);
AddRegExpCases( re_5, "\\B", false, false, false, Math.pow(2,30) );

/*
 * Brendan: "need to test cases Math.pow(2,32) and greater to see
 * whether they round-trip." Reason: thanks to the work done in
 * http://bugzilla.mozilla.org/show_bug.cgi?id=124339, lastIndex
 * is now stored as a double instead of a uint32 (unsigned integer).
 *
 * Note 2^32 -1 is the upper bound for uint32's, but doubles can go
 * all the way up to Number.MAX_VALUE. So that's why we need cases
 * between those two numbers.
 *
 */
re_6 = /\B/;
re_6.lastIndex = Math.pow(2,32);
AddRegExpCases( re_6, "\\B", false, false, false, Math.pow(2,32) );

re_7 = /\B/;
re_7.lastIndex = Math.pow(2,32) + 1;
AddRegExpCases( re_7, "\\B", false, false, false, Math.pow(2,32) + 1 );

re_8 = /\B/;
re_8.lastIndex = Math.pow(2,32) * 2;
AddRegExpCases( re_8, "\\B", false, false, false, Math.pow(2,32) * 2 );

re_9 = /\B/;
re_9.lastIndex = Math.pow(2,40);
AddRegExpCases( re_9, "\\B", false, false, false, Math.pow(2,40) );

re_10 = /\B/;
re_10.lastIndex = Number.MAX_VALUE;
AddRegExpCases( re_10, "\\B", false, false, false, Number.MAX_VALUE );



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function AddRegExpCases( re, s, g, i, m, l ){

  AddTestCase( re + ".test == RegExp.prototype.test",
	       true,
	       re.test == RegExp.prototype.test );

  AddTestCase( re + ".toString == RegExp.prototype.toString",
	       true,
	       re.toString == RegExp.prototype.toString );

  AddTestCase( re + ".contructor == RegExp.prototype.constructor",
	       true,
	       re.constructor == RegExp.prototype.constructor );

  AddTestCase( re + ".compile == RegExp.prototype.compile",
	       true,
	       re.compile == RegExp.prototype.compile );

  AddTestCase( re + ".exec == RegExp.prototype.exec",
	       true,
	       re.exec == RegExp.prototype.exec );

  // properties

  AddTestCase( re + ".source",
	       s,
	       re.source );

  AddTestCase( re + ".toString()",
	       "/" + s +"/" + (g?"g":"") + (i?"i":"") +(m?"m":""),
	       re.toString() );

  AddTestCase( re + ".global",
	       g,
	       re.global );

  AddTestCase( re + ".ignoreCase",
	       i,
	       re.ignoreCase );

  AddTestCase( re + ".multiline",
	       m,
	       re.multiline);

  AddTestCase( re + ".lastIndex",
	       l,
	       re.lastIndex  );
}

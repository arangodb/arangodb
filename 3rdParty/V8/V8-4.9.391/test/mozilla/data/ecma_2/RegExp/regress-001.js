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

gTestfile = 'regress-001.js';

/**
 *  File Name:          RegExp/regress-001.js
 *  ECMA Section:       N/A
 *  Description:        Regression test case:
 *  JS regexp anchoring on empty match bug
 *  http://bugzilla.mozilla.org/show_bug.cgi?id=2157
 *
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */
var SECTION = "RegExp/hex-001.js";
var VERSION = "ECMA_2";
var TITLE   = "JS regexp anchoring on empty match bug";
var BUGNUMBER = "2157";

startTest();

AddRegExpCases( /a||b/(''),
		"//a||b/('')",
		1,
		[''] );

test();

function AddRegExpCases( regexp, str_regexp, length, matches_array ) {

  AddTestCase(
    "( " + str_regexp + " ).length",
    regexp.length,
    regexp.length );


  for ( var matches = 0; matches < matches_array.length; matches++ ) {
    AddTestCase(
      "( " + str_regexp + " )[" + matches +"]",
      matches_array[matches],
      regexp[matches] );
  }
}

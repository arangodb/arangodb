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

gTestfile = 'hex-001.js';

/**
 *  File Name:          RegExp/hex-001.js
 *  ECMA Section:       15.7.3.1
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *  Positive test cases for constructing a RegExp object
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */
var SECTION = "RegExp/hex-001";
var VERSION = "ECMA_2";
var TITLE   = "RegExp patterns that contain HexicdecimalEscapeSequences";

startTest();

// These examples come from 15.7.1, HexidecimalEscapeSequence

AddRegExpCases( new RegExp("\x41"),  "new RegExp('\\x41')",  "A",  "A", 1, 0, ["A"] );
AddRegExpCases( new RegExp("\x412"),"new RegExp('\\x412')", "A2", "A2", 1, 0, ["A2"] );
AddRegExpCases( new RegExp("\x1g"), "new RegExp('\\x1g')",  "x1g","x1g", 1, 0, ["x1g"] );

AddRegExpCases( new RegExp("A"),  "new RegExp('A')",  "\x41",  "\\x41",  1, 0, ["A"] );
AddRegExpCases( new RegExp("A"),  "new RegExp('A')",  "\x412", "\\x412", 1, 0, ["A"] );
AddRegExpCases( new RegExp("^x"), "new RegExp('^x')", "x412",  "x412",   1, 0, ["x"]);
AddRegExpCases( new RegExp("A"),  "new RegExp('A')",  "A2",    "A2",     1, 0, ["A"] );

test();

function AddRegExpCases(
  regexp, str_regexp, pattern, str_pattern, length, index, matches_array ) {

  // prevent a runtime error

  if ( regexp.exec(pattern) == null || matches_array == null ) {
    AddTestCase(
      str_regexp + ".exec(" + pattern +")",
      matches_array,
      regexp.exec(pattern) );

    return;
  }

  AddTestCase(
    str_regexp + ".exec(" + str_pattern +").length",
    length,
    regexp.exec(pattern).length );

  AddTestCase(
    str_regexp + ".exec(" + str_pattern +").index",
    index,
    regexp.exec(pattern).index );

  AddTestCase(
    str_regexp + ".exec(" + str_pattern +").input",
    pattern,
    regexp.exec(pattern).input );

  for ( var matches = 0; matches < matches_array.length; matches++ ) {
    AddTestCase(
      str_regexp + ".exec(" + str_pattern +")[" + matches +"]",
      matches_array[matches],
      regexp.exec(pattern)[matches] );
  }
}

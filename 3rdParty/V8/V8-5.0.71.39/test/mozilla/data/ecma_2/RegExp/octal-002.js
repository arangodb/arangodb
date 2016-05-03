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

gTestfile = 'octal-002.js';

/**
 *  File Name:          RegExp/octal-002.js
 *  ECMA Section:       15.7.1
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *  Simple test cases for matching OctalEscapeSequences.
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */
var SECTION = "RegExp/octal-002.js";
var VERSION = "ECMA_2";
var TITLE   = "RegExp patterns that contain OctalEscapeSequences";
var BUGNUMBER="http://scopus/bugsplat/show_bug.cgi?id=346189";

startTest();

// backreference
AddRegExpCases(
  /(.)(.)(.)(.)(.)(.)(.)(.)\8/,
  "/(.)(.)(.)(.)(.)(.)(.)(.)\\8",
  "aabbccaaabbbccc",
  "aabbccaaabbbccc",
  0,
  ["aabbccaaa", "a", "a", "b", "b", "c", "c", "a", "a"] );

AddRegExpCases(
  /(.)(.)(.)(.)(.)(.)(.)(.)(.)\9/,
  "/(.)(.)(.)(.)(.)(.)(.)(.)\\9",
  "aabbccaabbcc",
  "aabbccaabbcc",
  0,
  ["aabbccaabb", "a", "a", "b", "b", "c", "c", "a", "a", "b"] );

AddRegExpCases(
  /(.)(.)(.)(.)(.)(.)(.)(.)(.)\8/,
  "/(.)(.)(.)(.)(.)(.)(.)(.)(.)\\8",
  "aabbccaababcc",
  "aabbccaababcc",
  0,
  ["aabbccaaba", "a", "a", "b", "b", "c", "c", "a", "a", "b"] );

test();

function AddRegExpCases(
  regexp, str_regexp, pattern, str_pattern, index, matches_array ) {

  // prevent a runtime error

  if ( regexp.exec(pattern) == null || matches_array == null ) {
    AddTestCase(
      regexp + ".exec(" + str_pattern +")",
      matches_array,
      regexp.exec(pattern) );

    return;
  }
  AddTestCase(
    str_regexp + ".exec(" + str_pattern +").length",
    matches_array.length,
    regexp.exec(pattern).length );

  AddTestCase(
    str_regexp + ".exec(" + str_pattern +").index",
    index,
    regexp.exec(pattern).index );

  AddTestCase(
    str_regexp + ".exec(" + str_pattern +").input",
    pattern,
    regexp.exec(pattern).input );

  AddTestCase(
    str_regexp + ".exec(" + str_pattern +").toString()",
    matches_array.toString(),
    regexp.exec(pattern).toString() );
/*
  var limit = matches_array.length > regexp.exec(pattern).length
  ? matches_array.length
  : regexp.exec(pattern).length;

  for ( var matches = 0; matches < limit; matches++ ) {
  AddTestCase(
  str_regexp + ".exec(" + str_pattern +")[" + matches +"]",
  matches_array[matches],
  regexp.exec(pattern)[matches] );
  }
*/
}

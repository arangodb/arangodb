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

gTestfile = 'match-002.js';

/**
 *  File Name:          String/match-002.js
 *  ECMA Section:       15.6.4.9
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */

/*
 *  String.match( regexp )
 *
 *  If regexp is not an object of type RegExp, it is replaced with result
 *  of the expression new RegExp(regexp). Let string denote the result of
 *  converting the this value to a string.  If regexp.global is false,
 *  return the result obtained by invoking RegExp.prototype.exec (see
 *  section 15.7.5.3) on regexp with string as parameter.
 *
 *  Otherwise, set the regexp.lastIndex property to 0 and invoke
 *  RegExp.prototype.exec repeatedly until there is no match. If there is a
 *  match with an empty string (in other words, if the value of
 *  regexp.lastIndex is left unchanged) increment regexp.lastIndex by 1.
 *  The value returned is an array with the properties 0 through n-1
 *  corresponding to the first element of the result of each matching
 *  invocation of RegExp.prototype.exec.
 *
 *  Note that the match function is intentionally generic; it does not
 *  require that its this value be a string object.  Therefore, it can be
 *  transferred to other kinds of objects for use as a method.
 *
 *  This file tests cases in which regexp.global is false.  Therefore,
 *  results should behave as regexp.exec with string passed as a parameter.
 *
 */

var SECTION = "String/match-002.js";
var VERSION = "ECMA_2";
var TITLE   = "String.prototype.match( regexp )";

startTest();

// the regexp argument is not a RegExp object
// this is not a string object

AddRegExpCases( /([\d]{5})([-\ ]?[\d]{4})?$/,
		"/([\d]{5})([-\ ]?[\d]{4})?$/",
		"Boston, Mass. 02134",
		14,
		["02134", "02134", undefined]);

AddGlobalRegExpCases( /([\d]{5})([-\ ]?[\d]{4})?$/g,
		      "/([\d]{5})([-\ ]?[\d]{4})?$/g",
		      "Boston, Mass. 02134",
		      ["02134"]);

// set the value of lastIndex
re = /([\d]{5})([-\ ]?[\d]{4})?$/;
re.lastIndex = 0;

s = "Boston, MA 02134";

AddRegExpCases( re,
		"re = /([\d]{5})([-\ ]?[\d]{4})?$/; re.lastIndex =0",
		s,
		s.lastIndexOf("0"),
		["02134", "02134", undefined]);


re.lastIndex = s.length;

AddRegExpCases( re,
		"re = /([\d]{5})([-\ ]?[\d]{4})?$/; re.lastIndex = " +
		s.length,
		s,
		s.lastIndexOf("0"),
		["02134", "02134", undefined] );

re.lastIndex = s.lastIndexOf("0");

AddRegExpCases( re,
		"re = /([\d]{5})([-\ ]?[\d]{4})?$/; re.lastIndex = " +
		s.lastIndexOf("0"),
		s,
		s.lastIndexOf("0"),
		["02134", "02134", undefined]);

re.lastIndex = s.lastIndexOf("0") + 1;

AddRegExpCases( re,
		"re = /([\d]{5})([-\ ]?[\d]{4})?$/; re.lastIndex = " +
		s.lastIndexOf("0") +1,
		s,
		s.lastIndexOf("0"),
		["02134", "02134", undefined]);

test();

function AddRegExpCases(
  regexp, str_regexp, string, index, matches_array ) {

  // prevent a runtime error

  if ( regexp.exec(string) == null || matches_array == null ) {
    AddTestCase(
      string + ".match(" + regexp +")",
      matches_array,
      string.match(regexp) );

    return;
  }

  AddTestCase(
    "( " + string  + " ).match(" + str_regexp +").length",
    matches_array.length,
    string.match(regexp).length );

  AddTestCase(
    "( " + string + " ).match(" + str_regexp +").index",
    index,
    string.match(regexp).index );

  AddTestCase(
    "( " + string + " ).match(" + str_regexp +").input",
    string,
    string.match(regexp).input );

  var limit = matches_array.length > string.match(regexp).length ?
    matches_array.length :
    string.match(regexp).length;

  for ( var matches = 0; matches < limit; matches++ ) {
    AddTestCase(
      "( " + string + " ).match(" + str_regexp +")[" + matches +"]",
      matches_array[matches],
      string.match(regexp)[matches] );
  }
}

function AddGlobalRegExpCases(
  regexp, str_regexp, string, matches_array ) {

  // prevent a runtime error

  if ( regexp.exec(string) == null || matches_array == null ) {
    AddTestCase(
      regexp + ".exec(" + string +")",
      matches_array,
      regexp.exec(string) );

    return;
  }

  AddTestCase(
    "( " + string  + " ).match(" + str_regexp +").length",
    matches_array.length,
    string.match(regexp).length );

  var limit = matches_array.length > string.match(regexp).length ?
    matches_array.length :
    string.match(regexp).length;

  for ( var matches = 0; matches < limit; matches++ ) {
    AddTestCase(
      "( " + string + " ).match(" + str_regexp +")[" + matches +"]",
      matches_array[matches],
      string.match(regexp)[matches] );
  }
}

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

gTestfile = 'split-003.js';

/**
 *  File Name:          String/split-003.js
 *  ECMA Section:       15.6.4.9
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */

/*
 * Since regular expressions have been part of JavaScript since 1.2, there
 * are already tests for regular expressions in the js1_2/regexp folder.
 *
 * These new tests try to supplement the existing tests, and verify that
 * our implementation of RegExp conforms to the ECMA specification, but
 * does not try to be as exhaustive as in previous tests.
 *
 * The [,limit] argument to String.split is new, and not covered in any
 * existing tests.
 *
 * String.split cases are covered in ecma/String/15.5.4.8-*.js.
 * String.split where separator is a RegExp are in
 * js1_2/regexp/string_split.js
 *
 */

var SECTION = "ecma_2/String/split-003.js";
var VERSION = "ECMA_2";
var TITLE   = "String.prototype.split( regexp, [,limit] )";

startTest();

// separator is a regexp
// separator regexp value global setting is set
// string is an empty string
// if separator is an empty string, split each by character


AddSplitCases( "hello", new RegExp, "new RegExp", ["h","e","l","l","o"] );

AddSplitCases( "hello", /l/, "/l/", ["he","","o"] );
AddLimitedSplitCases( "hello", /l/, "/l/", 0, [] );
AddLimitedSplitCases( "hello", /l/, "/l/", 1, ["he"] );
AddLimitedSplitCases( "hello", /l/, "/l/", 2, ["he",""] );
AddLimitedSplitCases( "hello", /l/, "/l/", 3, ["he","","o"] );
AddLimitedSplitCases( "hello", /l/, "/l/", 4, ["he","","o"] );
AddLimitedSplitCases( "hello", /l/, "/l/", void 0, ["he","","o"] );
AddLimitedSplitCases( "hello", /l/, "/l/", "hi", [] );
AddLimitedSplitCases( "hello", /l/, "/l/", undefined, ["he","","o"] );

AddSplitCases( "hello", new RegExp, "new RegExp", ["h","e","l","l","o"] );
AddLimitedSplitCases( "hello", new RegExp, "new RegExp", 0, [] );
AddLimitedSplitCases( "hello", new RegExp, "new RegExp", 1, ["h"] );
AddLimitedSplitCases( "hello", new RegExp, "new RegExp", 2, ["h","e"] );
AddLimitedSplitCases( "hello", new RegExp, "new RegExp", 3, ["h","e","l"] );
AddLimitedSplitCases( "hello", new RegExp, "new RegExp", 4, ["h","e","l","l"] );
AddLimitedSplitCases( "hello", new RegExp, "new RegExp", void 0,  ["h","e","l","l","o"] );
AddLimitedSplitCases( "hello", new RegExp, "new RegExp", "hi",  [] );
AddLimitedSplitCases( "hello", new RegExp, "new RegExp", undefined,  ["h","e","l","l","o"] );

test();

function AddSplitCases( string, separator, str_sep, split_array ) {
  // verify that the result of split is an object of type Array
  AddTestCase(
    "( " + string  + " ).split(" + str_sep +").constructor == Array",
    true,
    string.split(separator).constructor == Array );

  // check the number of items in the array
  AddTestCase(
    "( " + string  + " ).split(" + str_sep +").length",
    split_array.length,
    string.split(separator).length );

  // check the value of each array item
  var limit = (split_array.length > string.split(separator).length )
    ? split_array.length : string.split(separator).length;

  for ( var matches = 0; matches < split_array.length; matches++ ) {
    AddTestCase(
      "( " + string + " ).split(" + str_sep +")[" + matches +"]",
      split_array[matches],
      string.split( separator )[matches] );
  }
}

function AddLimitedSplitCases(
  string, separator, str_sep, limit, split_array ) {

  // verify that the result of split is an object of type Array

  AddTestCase(
    "( " + string  + " ).split(" + str_sep +", " + limit +
    " ).constructor == Array",
    true,
    string.split(separator, limit).constructor == Array );

  // check the length of the array

  AddTestCase(
    "( " + string + " ).split(" + str_sep  +", " + limit + " ).length",
    split_array.length,
    string.split(separator, limit).length );

  // check the value of each array item

  var slimit = (split_array.length > string.split(separator).length )
    ? split_array.length : string.split(separator, limit).length;

  for ( var matches = 0; matches < slimit; matches++ ) {
    AddTestCase(
      "( " + string + " ).split(" + str_sep +", " + limit + " )[" + matches +"]",
      split_array[matches],
      string.split( separator, limit )[matches] );
  }
}

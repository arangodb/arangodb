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

gTestfile = 'multiline-001.js';

/**
 *  File Name:          RegExp/multiline-001.js
 *  ECMA Section:
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *
 *  Date:               19 February 1999
 */

var SECTION = "RegExp/multiline-001";
var VERSION = "ECMA_2";
var TITLE   = "RegExp: multiline flag";
var BUGNUMBER="343901";

startTest();

var woodpeckers = "ivory-billed\ndowny\nhairy\nacorn\nyellow-bellied sapsucker\n" +
  "northern flicker\npileated\n";

AddRegExpCases( /.*[y]$/m, woodpeckers, woodpeckers.indexOf("downy"), ["downy"] );

AddRegExpCases( /.*[d]$/m, woodpeckers, woodpeckers.indexOf("ivory-billed"), ["ivory-billed"] );

test();


function AddRegExpCases
( regexp, pattern, index, matches_array ) {

  // prevent a runtime error

  if ( regexp.exec(pattern) == null || matches_array == null ) {
    AddTestCase(
      regexp + ".exec(" + pattern +")",
      matches_array,
      regexp.exec(pattern) );

    return;
  }

  AddTestCase(
    regexp.toString() + ".exec(" + pattern +").length",
    matches_array.length,
    regexp.exec(pattern).length );

  AddTestCase(
    regexp.toString() + ".exec(" + pattern +").index",
    index,
    regexp.exec(pattern).index );

  AddTestCase(
    regexp + ".exec(" + pattern +").input",
    pattern,
    regexp.exec(pattern).input );


  for ( var matches = 0; matches < matches_array.length; matches++ ) {
    AddTestCase(
      regexp + ".exec(" + pattern +")[" + matches +"]",
      matches_array[matches],
      regexp.exec(pattern)[matches] );
  }
}

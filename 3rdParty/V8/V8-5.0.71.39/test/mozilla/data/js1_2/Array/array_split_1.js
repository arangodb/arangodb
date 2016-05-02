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

gTestfile = 'array_split_1.js';

/**
   File Name:          array_split_1.js
   ECMA Section:       Array.split()
   Description:

   These are tests from free perl suite.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "Free Perl";
var VERSION = "JS1_2";
var TITLE   = "Array.split()";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase( SECTION,
	      "('a,b,c'.split(',')).length",
	      3,
	      ('a,b,c'.split(',')).length );

new TestCase( SECTION,
	      "('a,b'.split(',')).length",
	      2,
	      ('a,b'.split(',')).length );

new TestCase( SECTION,
	      "('a'.split(',')).length",
	      1,
	      ('a'.split(',')).length );

/*
 * Deviate from ECMA by never splitting an empty string by any separator
 * string into a non-empty array (an array of length 1 that contains the
 * empty string).
 */
new TestCase( SECTION,
	      "(''.split(',')).length",
	      0,
	      (''.split(',')).length );

test();

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

gTestfile = 'label-002.js';

/**
 *  File Name:          label-002.js
 *  ECMA Section:
 *  Description:        Labeled statements
 *
 *  Labeled break and continue within a for-in loop.
 *
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "label-002";
var VERSION = "ECMA_2";
var TITLE   = "Labeled statements";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

LabelTest( { p1:"hi,", p2:" norris" }, "hi, norris", " norrishi," );
LabelTest( { 0:"zero", 1:"one" }, "zeroone", "onezero" );

LabelTest2( { p1:"hi,", p2:" norris" }, "hi,", " norris" );
LabelTest2( { 0:"zero", 1:"one" }, "zero", "one" );

test();

function LabelTest( object, expect1, expect2 ) {
  result = "";

yoohoo:  { for ( property in object ) { result += object[property]; }; break yoohoo };

  new TestCase(
    SECTION,
    "yoohoo: for ( property in object ) { result += object[property]; } break yoohoo }",
    true,
    result == expect1 || result == expect2 );
}

function LabelTest2( object, expect1, expect2 ) {
  result = "";

yoohoo:  { for ( property in object ) { result += object[property]; break yoohoo } }; ;

  new TestCase(
    SECTION,
    "yoohoo: for ( property in object ) { result += object[property]; break yoohoo }}",
    true,
    result == expect1 || result == expect2 );
}


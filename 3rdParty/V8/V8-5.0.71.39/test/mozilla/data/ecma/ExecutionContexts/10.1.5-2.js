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

gTestfile = '10.1.5-2.js';

/**
   File Name:          10.1.5-2.js
   ECMA Section:       10.1.5 Global Object
   Description:
   There is a unique global object which is created before control enters
   any execution context. Initially the global object has the following
   properties:

   Built-in objects such as Math, String, Date, parseInt, etc. These have
   attributes { DontEnum }.

   Additional host defined properties. This may include a property whose
   value is the global object itself, for example window in HTML.

   As control enters execution contexts, and as ECMAScript code is executed,
   additional properties may be added to the global object and the initial
   properties may be changed.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "10.5.1-2";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Global Object");

new TestCase( "SECTION", "Eval Code check" );

var EVAL_STRING = 'if ( Object == null ) { gTestcases[0].reason += " Object == null" ; }' +
  'if ( Function == null ) { gTestcases[0].reason += " Function == null"; }' +
  'if ( String == null ) { gTestcases[0].reason += " String == null"; }'   +
  'if ( Array == null ) { gTestcases[0].reason += " Array == null"; }'     +
  'if ( Number == null ) { gTestcases[0].reason += " Function == null";}'  +
  'if ( Math == null ) { gTestcases[0].reason += " Math == null"; }'       +
  'if ( Boolean == null ) { gTestcases[0].reason += " Boolean == null"; }' +
  'if ( Date  == null ) { gTestcases[0].reason += " Date == null"; }'      +
  'if ( eval == null ) { gTestcases[0].reason += " eval == null"; }'       +
  'if ( parseInt == null ) { gTestcases[0].reason += " parseInt == null"; }' ;

eval( EVAL_STRING );

/*
  if ( NaN == null ) {
  gTestcases[0].reason += " NaN == null";
  }
  if ( Infinity == null ) {
  gTestcases[0].reason += " Infinity == null";
  }
*/

if ( gTestcases[0].reason != "" ) {
  gTestcases[0].actual = "fail";
} else {
  gTestcases[0].actual = "pass";
}
gTestcases[0].expect = "pass";

test();


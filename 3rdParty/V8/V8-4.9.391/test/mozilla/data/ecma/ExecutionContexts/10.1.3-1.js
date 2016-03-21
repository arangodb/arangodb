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

gTestfile = '10.1.3-1.js';

/**
   File Name:          10.1.3-1.js
   ECMA Section:       10.1.3
   Description:

   For each formal parameter, as defined in the FormalParameterList, create
   a property of the variable object whose name is the Identifier and whose
   attributes are determined by the type of code. The values of the
   parameters are supplied by the caller. If the caller supplies fewer
   parameter values than there are formal parameters, the extra formal
   parameters have value undefined. If two or more formal parameters share
   the same name, hence the same property, the corresponding property is
   given the value that was supplied for the last parameter with this name.
   If the value of this last parameter was not supplied by the caller,
   the value of the corresponding property is undefined.


   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=104191

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "10.1.3-1";
var VERSION = "ECMA_1";
var TITLE   = "Variable Instantiation:  Formal Parameters";
var BUGNUMBER="104191";
startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

var myfun1 = new Function( "a", "a", "return a" );
var myfun2 = new Function( "a", "b", "a", "return a" );

function myfun3(a, b, a) {
  return a;
}

// myfun1, myfun2, myfun3 tostring


new TestCase(
  SECTION,
  String(myfun2) +"; myfun2(2,4,8)",
  8,
  myfun2(2,4,8) );

new TestCase(
  SECTION,
  "myfun2(2,4)",
  void 0,
  myfun2(2,4));

new TestCase(
  SECTION,
  String(myfun3) +"; myfun3(2,4,8)",
  8,
  myfun3(2,4,8) );

new TestCase(
  SECTION,
  "myfun3(2,4)",
  void 0,
  myfun3(2,4) );

test();


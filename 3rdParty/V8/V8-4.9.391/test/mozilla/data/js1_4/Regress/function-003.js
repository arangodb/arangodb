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

gTestfile = 'function-003.js';

/**
 *  File Name:          function-003.js
 *  Description:
 *
 *  http://scopus.mcom.com/bugsplat/show_bug.cgi?id=104766
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "toString-001.js";
var VERSION = "JS1_4";
var TITLE   = "Regression test case for 104766";
var BUGNUMBER="310514";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(
  SECTION,
  "StripSpaces(Array.prototype.concat.toString()).substring(0,17)",
  "functionconcat(){",
  StripSpaces(Array.prototype.concat.toString()).substring(0,17));

test();

function StripSpaces( s ) {
  for ( var currentChar = 0, strippedString="";
        currentChar < s.length; currentChar++ )
  {
    if (!IsWhiteSpace(s.charAt(currentChar))) {
      strippedString += s.charAt(currentChar);
    }
  }
  return strippedString;
}

function IsWhiteSpace( string ) {
  var cc = string.charCodeAt(0);
  switch (cc) {
  case (0x0009):
  case (0x000B):
  case (0x000C):
  case (0x0020):
  case (0x000A):
  case (0x000D):
  case ( 59 ): // let's strip out semicolons, too
    return true;
    break;
  default:
    return false;
  }
}


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

gTestfile = 'switch-004.js';

/**
 *  File Name:          switch-003.js
 *  ECMA Section:
 *  Description:        The switch Statement
 *
 *  This uses variables and objects as case expressions in switch statements.
 * This verifies a bunch of bugs:
 *
 * http://scopus.mcom.com/bugsplat/show_bug.cgi?id=315988
 * http://scopus.mcom.com/bugsplat/show_bug.cgi?id=315975
 * http://scopus.mcom.com/bugsplat/show_bug.cgi?id=315954
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 *
 */
var SECTION = "switch-003";
var VERSION = "ECMA_2";
var TITLE   = "The switch statement";
var BUGNUMBER= "315988";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

ONE = new Number(1);
ZERO = new Number(0);
var A = new String("A");
var B = new String("B");
TRUE = new Boolean( true );
FALSE = new Boolean( false );
UNDEFINED  = void 0;
NULL = null;

SwitchTest( ZERO, "ZERO" );
SwitchTest( NULL, "NULL" );
SwitchTest( UNDEFINED, "UNDEFINED" );
SwitchTest( FALSE, "FALSE" );
SwitchTest( false,  "false" );
SwitchTest( 0,      "0" );

SwitchTest ( TRUE, "TRUE" );
SwitchTest( 1,     "1" );
SwitchTest( ONE,   "ONE" );
SwitchTest( true,  "true" );

SwitchTest( "a",   "a" );
SwitchTest( A,     "A" );
SwitchTest( "b",   "b" );
SwitchTest( B,     "B" );

SwitchTest( new Boolean( true ), "default" );
SwitchTest( new Boolean(false ), "default" );
SwitchTest( new String( "A" ),   "default" );
SwitchTest( new Number( 0 ),     "default" );

test();

function SwitchTest( input, expect ) {
  var result = "";

  switch ( input ) {
  default:   result += "default"; break;
  case "a":  result += "a";       break;
  case "b":  result += "b";       break;
  case A:    result += "A";       break;
  case B:    result += "B";       break;
  case new Boolean(true): result += "new TRUE";   break;
  case new Boolean(false): result += "new FALSE"; break;
  case NULL: result += "NULL";    break;
  case UNDEFINED: result += "UNDEFINED"; break;
  case true: result += "true";    break;
  case false: result += "false";  break;
  case TRUE:  result += "TRUE";   break;
  case FALSE: result += "FALSE";  break;
  case 0:    result += "0";       break;
  case 1:    result += "1";       break;
  case new Number(0) : result += "new ZERO";  break;
  case new Number(1) : result += "new ONE";   break;
  case ONE:  result += "ONE";     break;
  case ZERO: result += "ZERO";    break;
  }

  new TestCase(
    SECTION,
    "switch with no breaks:  input is " + input,
    expect,
    result );
}

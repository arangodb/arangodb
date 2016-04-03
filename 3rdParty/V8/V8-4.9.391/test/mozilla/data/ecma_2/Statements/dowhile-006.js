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

gTestfile = 'dowhile-006.js';

/**
 *  File Name:          dowhile-006
 *  ECMA Section:
 *  Description:        do...while statements
 *
 *  A general do...while test.
 *
 *  Author:             christine@netscape.com
 *  Date:               26 August 1998
 */
var SECTION = "dowhile-006";
var VERSION = "ECMA_2";
var TITLE   = "do...while";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

DoWhile( new DoWhileObject( false, false, 10 ) );
DoWhile( new DoWhileObject( true, false, 2 ) );
DoWhile( new DoWhileObject( false, true, 3 ) );
DoWhile( new DoWhileObject( true, true, 4 ) );

test();

function looping( object ) {
  object.iterations--;

  if ( object.iterations <= 0 ) {
    return false;
  } else {
    return true;
  }
}
function DoWhileObject( breakOut, breakIn, iterations, loops ) {
  this.iterations = iterations;
  this.loops = loops;
  this.breakOut = breakOut;
  this.breakIn  = breakIn;
  this.looping  = looping;
}
function DoWhile( object ) {
  var result1 = false;
  var result2 = false;

outie: {
  innie: {
      do {
	if ( object.breakOut )
	  break outie;

	if ( object.breakIn )
	  break innie;

      } while ( looping(object) );

      //  statements should be executed if:
      //  do...while exits normally
      //  do...while exits abruptly with no label

      result1 = true;

    }

//  statements should be executed if:
//  do...while breaks out with label "innie"
//  do...while exits normally
//  do...while does not break out with "outie"

    result2 = true;
  }

  new TestCase(
    SECTION,
    "hit code after loop in inner loop",
    ( object.breakIn || object.breakOut ) ? false : true ,
    result1 );

  new TestCase(
    SECTION,
    "hit code after loop in outer loop",
    ( object.breakOut ) ? false : true,
    result2 );
}

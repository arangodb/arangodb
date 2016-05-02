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

gTestfile = 'StrictEquality-001.js';

/**
 *  File Name:          StrictEquality-001.js
 *  ECMA Section:       11.9.6.js
 *  Description:
 *
 *  Author:             christine@netscape.com
 *  Date:               4 september 1998
 */
var SECTION = "StrictEquality-001 - 11.9.6";
var VERSION = "ECMA_2";
var TITLE   =  "The strict equality operator ( === )";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);


// 1. If Type(x) is different from Type(y) return false

StrictEquality( true, new Boolean(true), false );
StrictEquality( new Boolean(), false, false );
StrictEquality( "", new String(),    false );
StrictEquality( new String("hi"), "hi", false );

// 2. If Type(x) is not Number go to step 9.

// 3. If x is NaN, return false
StrictEquality( NaN, NaN,   false );
StrictEquality( NaN, 0,     false );

// 4. If y is NaN, return false.
StrictEquality( 0,  NaN,    false );

// 5. if x is the same number value as y, return true

// 6. If x is +0 and y is -0, return true

// 7. If x is -0 and y is +0, return true

// 8. Return false.


// 9.  If Type(x) is String, then return true if x and y are exactly
//  the same sequence of characters ( same length and same characters
//  in corresponding positions.) Otherwise return false.

//  10. If Type(x) is Boolean, return true if x and y are both true or
//  both false. otherwise return false.


//  Return true if x and y refer to the same object.  Otherwise return
//  false.

// Return false.


test();

function StrictEquality( x, y, expect ) {
  result = ( x === y );

  new TestCase(
    SECTION,
    x +" === " + y,
    expect,
    result );
}


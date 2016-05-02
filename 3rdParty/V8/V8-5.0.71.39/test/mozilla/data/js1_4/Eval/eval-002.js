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

gTestfile = 'eval-002.js';

/**
 *  File Name:   eval-002.js
 *  Description:  (SEE REVISED DESCRIPTION FURTHER BELOW)
 *
 *  The global eval function may not be accessed indirectly and then called.
 *  This feature will continue to work in JavaScript 1.3 but will result in an
 *  error in JavaScript 1.4. This restriction is also in place for the With and
 *  Closure constructors.
 *
 *  http://scopus.mcom.com/bugsplat/show_bug.cgi?id=324451
 *
 *  Author:          christine@netscape.com
 *  Date:               11 August 1998
 *
 *
 *  REVISION:    05  February 2001
 *  Author:          pschwartau@netscape.com
 * 
 *  Indirect eval IS NOT ILLEGAL per ECMA3!!!  See
 *
 *  http://bugzilla.mozilla.org/show_bug.cgi?id=38512
 *
 *  ------- Additional Comments From Brendan Eich 2001-01-30 17:12 -------
 * ECMA-262 Edition 3 doesn't require implementations to throw EvalError,
 * see the short, section-less Chapter 16.  It does say an implementation that
 * doesn't throw EvalError must allow assignment to eval and indirect calls
 * of the evalnative method.
 *
 */
var SECTION = "eval-002.js";
var VERSION = "JS1_4";
var TITLE   = "Calling eval indirectly should NOT fail in version 140";
var BUGNUMBER="38512";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

var MY_EVAL = eval;
var RESULT = "";
var EXPECT = 1 + "testString"

  EvalTest();

test();


function EvalTest()
{   
  MY_EVAL( "RESULT = EXPECT" );

  new TestCase(
    SECTION,
    "Call eval indirectly",
    EXPECT,
    RESULT );
}


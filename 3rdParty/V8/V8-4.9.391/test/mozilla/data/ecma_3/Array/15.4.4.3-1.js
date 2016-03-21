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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   pschwartau@netscape.com
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

/*
 * Date: 12 Mar 2001
 *
 *
 * SUMMARY: Testing Array.prototype.toLocaleString()
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=56883
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=58031
 *
 * By ECMA3 15.4.4.3, myArray.toLocaleString() means that toLocaleString()
 * should be applied to each element of the array, and the results should be
 * concatenated with an implementation-specific delimiter. For example:
 *
 *  myArray[0].toLocaleString()  +  ','  +  myArray[1].toLocaleString()  + etc.
 *
 * In this testcase toLocaleString is a user-defined property of each
 * array element; therefore it is the function that should be
 * invoked. This function increments a global variable. Therefore the
 * end value of this variable should be myArray.length.
 */
//-----------------------------------------------------------------------------
var gTestfile = '15.4.4.3-1.js';
var BUGNUMBER = 56883;
var summary = 'Testing Array.prototype.toLocaleString() -';
var actual = '';
var expect = '';
var n = 0;
var obj = {toLocaleString: function() {n++}};
var myArray = [obj, obj, obj];


myArray.toLocaleString();
actual = n;
expect = 3; // (see explanation above)


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Michael Daumling <daumling@adobe.com>
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

var gTestfile = 'regress-315974.js';
//-----------------------------------------------------------------------------

/*
  This test case uses String.quote(), because that method uses the JS printf()
  functions internally. The printf() functions print to a char* buffer, causing
  conversion to and from UTF-8 if UTF-8 is enabled. If not, UTF-8 sequences are
  converted to ASCII \uXXXX sequences. Thus, both return values are acceptable.
*/

var BUGNUMBER = 315974;
var summary = 'Test internal printf() for wide characters';

printBugNumber(BUGNUMBER);
printStatus (summary);

enterFunc (String (BUGNUMBER));

var goodSurrogatePair = '\uD841\uDC42';
var badSurrogatePair = '\uD841badbad';

var goodSurrogatePairQuotedUtf8 = '"\uD841\uDC42"';
var badSurrogatePairQuotedUtf8 = 'no error thrown!';
var goodSurrogatePairQuotedNoUtf8 = '"\\uD841\\uDC42"';
var badSurrogatePairQuotedNoUtf8 = '"\\uD841badbad"';

var status = summary + ': String.quote() should pay respect to surrogate pairs';
var actual = goodSurrogatePair.quote();
/* Result in case UTF-8 is enabled. */
var expect = goodSurrogatePairQuotedUtf8;
/* Result in case UTF-8 is disabled. */
if (actual != expect && actual == goodSurrogatePairQuotedNoUtf8)
  expect = actual;
reportCompare(expect, actual, status);

/*
 * A malformed surrogate pair throws an out-of-memory error,
 * but only if UTF-8 is enabled.
 */
status = summary + ': String.quote() should throw error on bad surrogate pair';
/* Out of memory is not catchable. */
actual = badSurrogatePair.quote();
/* Come here only if UTF-8 is disabled. */
reportCompare(badSurrogatePairQuotedNoUtf8, actual, status);

exitFunc (String (BUGNUMBER));

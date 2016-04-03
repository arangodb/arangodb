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

var gTestfile = 'regress-301738-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 301738;
var summary = 'Date parse compatibilty with MSIE';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

/* 
    Case 1. The input string contains an English month name.
    The form of the string can be month f l, or f month l, or
    f l month which each evaluate to the same date.
    If f and l are both greater than or equal to 70, or
    both less than 70, the date is invalid.
    The year is taken to be the greater of the values f, l.
    If the year is greater than or equal to 70 and less than 100,
    it is considered to be the number of years after 1900.
*/

var month = 'January';
var f;
var l;

f = l = 0;
expect = true;

actual = isNaN(new Date(month + ' ' + f + ' ' + l));
reportCompare(expect, actual, 'January 0 0 is invalid');

actual = isNaN(new Date(f + ' ' + l + ' ' + month));
reportCompare(expect, actual, '0 0 January is invalid');

actual = isNaN(new Date(f + ' ' + month + ' ' + l));
reportCompare(expect, actual, '0 January 0 is invalid');

f = l = 70;

actual = isNaN(new Date(month + ' ' + f + ' ' + l));
reportCompare(expect, actual, 'January 70 70 is invalid');

actual = isNaN(new Date(f + ' ' + l + ' ' + month));
reportCompare(expect, actual, '70 70 January is invalid');

actual = isNaN(new Date(f + ' ' + month + ' ' + l));
reportCompare(expect, actual, '70 January 70 is invalid');

f = 100;
l = 15;

// year, month, day
expect = new Date(f, 0, l).toString();

actual = new Date(month + ' ' + f + ' ' + l).toString();
reportCompare(expect, actual, 'month f l');

actual = new Date(f + ' ' + l + ' ' + month).toString();
reportCompare(expect, actual, 'f l month');

actual = new Date(f + ' ' + month + ' ' + l).toString();
reportCompare(expect, actual, 'f month l');

f = 80;
l = 15;

// year, month, day
expect = (new Date(f, 0, l)).toString();

actual = (new Date(month + ' ' + f + ' ' + l)).toString();
reportCompare(expect, actual, 'month f l');

actual = (new Date(f + ' ' + l + ' ' + month)).toString();
reportCompare(expect, actual, 'f l month');

actual = (new Date(f + ' ' + month + ' ' + l)).toString();
reportCompare(expect, actual, 'f month l');

f = 2040;
l = 15;

// year, month, day
expect = (new Date(f, 0, l)).toString();

actual = (new Date(month + ' ' + f + ' ' + l)).toString();
reportCompare(expect, actual, 'month f l');

actual = (new Date(f + ' ' + l + ' ' + month)).toString();
reportCompare(expect, actual, 'f l month');

actual = (new Date(f + ' ' + month + ' ' + l)).toString();
reportCompare(expect, actual, 'f month l');


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

var gTestfile = 'regress-301738-02.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 301738;
var summary = 'Date parse compatibilty with MSIE';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

/* 
    Case 2. The input string is of the form "f/m/l" where f, m and l are
    integers, e.g. 7/16/45.
    Adjust the mon, mday and year values to achieve 100% MSIE
    compatibility.
    a. If 0 <= f < 70, f/m/l is interpreted as month/day/year.
    i.  If year < 100, it is the number of years after 1900
    ii. If year >= 100, it is the number of years after 0.
    b. If 70 <= f < 100
    i.  If m < 70, f/m/l is interpreted as
    year/month/day where year is the number of years after
    1900.
    ii. If m >= 70, the date is invalid.
    c. If f >= 100
    i.  If m < 70, f/m/l is interpreted as
    year/month/day where year is the number of years after 0.
    ii. If m >= 70, the date is invalid.
*/

var f;
var m;
var l;

function newDate(f, m, l)
{
  return new Date(f + '/' + m + '/' + l);
}

function newDesc(f, m, l)
{
  return f + '/' + m + '/' + l;
}

// 2.a.i
f = 0;
m = 0;
l = 0;

expect = (new Date(l, f-1, m)).toDateString();
actual = newDate(f, m, l).toDateString();
reportCompare(expect, actual, newDesc(f, m, l));

f = 0;
m = 0;
l = 100;

expect = (new Date(l, f-1, m)).toDateString();
actual = newDate(f, m, l).toDateString();
reportCompare(expect, actual, newDesc(f, m, l));

// 2.a.ii
f = 0;
m = 24;
l = 100;

expect = (new Date(l, f-1, m)).toDateString();
actual = newDate(f, m, l).toDateString();
reportCompare(expect, actual, newDesc(f, m, l));

f = 0;
m = 24;
l = 2100;

expect = (new Date(l, f-1, m)).toDateString();
actual = newDate(f, m, l).toDateString();
reportCompare(expect, actual, newDesc(f, m, l));


// 2.b.i
f = 70;
m = 24;
l = 100;

expect = (new Date(f, m-1, l)).toDateString();
actual = newDate(f, m, l).toDateString();
reportCompare(expect, actual, newDesc(f, m, l));

f = 99;
m = 12;
l = 1;

expect = (new Date(f, m-1, l)).toDateString();
actual = newDate(f, m, l).toDateString();
reportCompare(expect, actual, newDesc(f, m, l));

// 2.b.ii.

f = 99;
m = 70;
l = 1;

expect = true;
actual = isNaN(newDate(f, m, l));
reportCompare(expect, actual, newDesc(f, m, l) + ' is an invalid date');

// 2.c.i

f = 100;
m = 12;
l = 1;

expect = (new Date(f, m-1, l)).toDateString();
actual = newDate(f, m, l).toDateString();
reportCompare(expect, actual, newDesc(f, m, l));

// 2.c.ii

f = 100;
m = 70;
l = 1;

expect = true;
actual = isNaN(newDate(f, m, l));
reportCompare(expect, actual, newDesc(f, m, l) + ' is an invalid date');



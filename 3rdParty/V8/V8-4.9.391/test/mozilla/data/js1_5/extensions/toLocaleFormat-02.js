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
 * Contributor(s): Mike Shaver
 *                 Bob Clary
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

var gTestfile = 'toLocaleFormat-02.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 291494;
var summary = 'Date.prototype.toLocaleFormat extension';
var actual = '';
var expect = '';
var temp;

/*
 * SpiderMonkey only.
 *
 * This test uses format strings which are not supported cross
 * platform and are expected to fail on at least some platforms
 * however they all currently pass on Linux (Fedora Core 6). They are
 * included here in order to increase coverage for cases where a crash
 * may occur.  These failures will be tracked in the
 * mozilla/js/tests/public-failures.txt list.
 * 
 */

enterFunc ('test');
printBugNumber(BUGNUMBER);
printStatus (summary);
 
var date = new Date("06/05/2005 00:00:00 GMT-0000");

expect = '20';
actual = date.toLocaleFormat('%C');
reportCompare(expect, actual, 'Date.toLocaleFormat("%C")');

expect = date.toLocaleFormat('%C%y');
actual = date.toLocaleFormat('%Y');
reportCompare(expect, actual, 'Date.toLocaleFormat("%C%y") == ' +
              'Date.toLocaleFormat("%Y")');

expect = date.toLocaleFormat('%m/%d/%y');
actual = date.toLocaleFormat('%D');
reportCompare(expect, actual, 'Date.toLocaleFormat("%m/%d/%y") == ' +
              'Date.toLocaleFormat("%D")');

expect = (date.getTimezoneOffset() > 0) ? ' 4' : ' 5';
actual = date.toLocaleFormat('%e');
reportCompare(expect, actual, 'Date.toLocaleFormat("%e")');

expect = date.toLocaleFormat('%Y-%m-%d');
actual = date.toLocaleFormat('%F');
reportCompare(expect, actual, 'Date.toLocaleFormat("%Y-%m-%d") == ' +
              'Date.toLocaleFormat("%F")');

expect = '05';
actual = date.toLocaleFormat('%g');
reportCompare(expect, actual, 'Date.toLocaleFormat("%g")');

expect = '2005';
actual = date.toLocaleFormat('%G');
reportCompare(expect, actual, 'Date.toLocaleFormat("%G")');

expect = date.toLocaleFormat('%b');
actual = date.toLocaleFormat('%h');
reportCompare(expect, actual, 'Date.toLocaleFormat("%b") == ' +
              'Date.toLocaleFormat("%h")');

expect = '\n';
actual = date.toLocaleFormat('%n');
reportCompare(expect, actual, 'Date.toLocaleFormat("%n") == "\\n"');

expect = date.toLocaleFormat('%I:%M:%S %p');
actual = date.toLocaleFormat('%r');
reportCompare(expect, actual, 'Date.toLocaleFormat("%I:%M:%S %p") == ' +
              'Date.toLocaleFormat("%r")');

expect = date.toLocaleFormat('%H:%M');
actual = date.toLocaleFormat('%R');
reportCompare(expect, actual, 'Date.toLocaleFormat("%H:%M") == ' +
              'Date.toLocaleFormat("%R")');

expect = '\t';
actual = date.toLocaleFormat('%t');
reportCompare(expect, actual, 'Date.toLocaleFormat("%t") == "\\t"');

expect = date.toLocaleFormat('%H:%M:%S');
actual = date.toLocaleFormat('%T');
reportCompare(expect, actual, 'Date.toLocaleFormat("%H:%M:%S") == ' +
              'Date.toLocaleFormat("%T")');

expect = String(6 + ((date.getTimezoneOffset() > 0) ? 0 : 1));
actual = date.toLocaleFormat('%u');
reportCompare(expect, actual, 'Date.toLocaleFormat("%u")');

expect = '22';
actual = date.toLocaleFormat('%V');
reportCompare(expect, actual, 'Date.toLocaleFormat("%V")');

print('Note: For Date.toLocaleFormat("%m/%d/%y") == Date.toLocaleFormat("%x") ' + 
      'to pass in Windows, the Regional Setting for the short date must be ' + 
      'set to mm/dd/yyyy');
expect = date.toLocaleFormat('%m/%d/%Y');
actual = date.toLocaleFormat('%x');
reportCompare(expect, actual, 'Date.toLocaleFormat("%m/%d/%Y") == ' +
              'Date.toLocaleFormat("%x")');

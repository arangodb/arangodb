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
 * Contributor(s): Bob Clary
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

var gTestfile = '15.9.3.2-1.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 273292;
var summary = '15.9.3.2  new Date(value)';
var actual = '';
var expect = '';
var date1;
var date2;
var i;
var validDateStrings = [
  "11/69/2004",
  "11/70/2004",
  "69/69/2004",
  "69/69/69",
  "69/69/1969",
  "70/69/70",
  "70/69/1970",
  "70/69/2004"
  ];

var invalidDateStrings = [
  "70/70/70",
  "70/70/1970",
  "70/70/2004"
  ];

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = 0;

for (i = 0; i < validDateStrings.length; i++)
{
  date1 = new Date(validDateStrings[i]);
  date2 = new Date(date1.toDateString());
  actual = date2 - date1;

  reportCompare(expect, actual, inSection(i) + ' ' +
		validDateStrings[i]);
}

expect = true;

var offset = validDateStrings.length;

for (i = 0; i < invalidDateStrings.length; i++)
{
  date1 = new Date(invalidDateStrings[i]);
  actual = isNaN(date1);

  reportCompare(expect, actual, inSection(i + offset) + ' ' +
		invalidDateStrings[i] + ' is invalid.');
}


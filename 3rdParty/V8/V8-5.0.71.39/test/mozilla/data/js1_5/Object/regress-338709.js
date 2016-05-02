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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Seno Aiko
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

var gTestfile = 'regress-338709.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 338709;
var summary = 'ReadOnly properties should not be overwritten by using ' +
  'Object and try..throw..catch';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

Object = function () { return Math };
expect = Math.LN2;
try
{
  throw 1990;
}
catch (LN2)
{
}
actual = Math.LN2;
print("Math.LN2 = " + Math.LN2)
  reportCompare(expect, actual, summary);

var s = new String("abc");
Object = function () { return s };
expect = s.length;
try
{
  throw -8
    }
catch (length)
{
}
actual = s.length;
print("length of '" + s + "' = " + s.length)
  reportCompare(expect, actual, summary);

var re = /xy/m;
Object = function () { return re };
expect = re.multiline;
try
{
  throw false
    }
catch (multiline)
{
}
actual = re.multiline;
print("re.multiline = " + re.multiline)
  reportCompare(expect, actual, summary);

if ("document" in this) {
  // Let the document be its own documentElement.
  Object = function () { return document }
  expect = document.documentElement + '';
  try
  {
    throw document;
  }
  catch (documentElement)
  {
  }
  actual = document.documentElement + '';
  print("document.documentElement = " + document.documentElement)
    }
else
  Object = this.constructor
 
    reportCompare(expect, actual, summary);

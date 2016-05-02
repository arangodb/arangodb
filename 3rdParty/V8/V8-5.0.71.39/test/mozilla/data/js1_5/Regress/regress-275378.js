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
 * Contributor(s): Martin Zvieger <martin.zvieger@sphinx.at>
 *                 Bob Clary <bob@bclary.com>
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

var gTestfile = 'regress-275378.js';
//-----------------------------------------------------------------------------
// testcase by Martin Zvieger <martin.zvieger@sphinx.at>
// if fails, will fail to run in browser due to syntax error
var BUGNUMBER = 275378;
var summary = 'Literal RegExp in case block should not give syntax error';
var actual = '';
var expect = '';

var status;

printBugNumber(BUGNUMBER);
printStatus (summary);


var tmpString= "XYZ";
// works
/ABC/.test(tmpString);
var tmpVal= 1;
if (tmpVal == 1)
{
  // works
  /ABC/.test(tmpString);
}
switch(tmpVal)
{
case 1:
{
  // works
  /ABC/.test(tmpString);
}
break;
}
switch(tmpVal)
{
case 1:
  // fails with syntax error
  /ABC/.test(tmpString);
  break;
}

expect = actual = 'no error';
reportCompare(expect, actual, status);

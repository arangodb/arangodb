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
 * Contributor(s): Bob Clary <bob@bclary.com>
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

var gTestfile = 'regress-169559.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 169559;
var summary = 'Global vars should not be more than 2.5 times slower than local';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var starttime;
var stoptime;
var globaltime = 0;
var localtime  = 0;
var maxratio   = 2.5;
var ratio;

var globalvar;
var globaltotal = 0;

printStatus("Testing global variables");
starttime = new Date();
for (globalvar = 0; globalvar < 100000; globalvar++)
{
  globaltotal += 1;
}
stoptime = new Date();
globaltime = stoptime - starttime;

printStatus("Testing local variables");
starttime= new Date();
local();
stoptime = new Date();
localtime = stoptime - starttime;

ratio = globaltime/localtime;
printStatus("Ratio of global to local time " + ratio.toFixed(3));

expect = true;
actual = (ratio < maxratio);
summary += ', Ratio: ' + ratio + ' '; 
reportCompare(expect, actual, summary);

function local()
{
  var localvar;
  var localtotal = 0;

  for (localvar = 0; localvar < 100000; localvar++)
  {
    localtotal += 1;
  }

}

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
 * Contributor(s): Bug Tracker
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

var gTestfile = 'regress-289669.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 289669;
var summary = 'O(N^2) behavior on String.replace(/RegExp/, ...)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);


var data = {X: [], Y:[]};

function replace(str) {
  var stra=str.replace(new RegExp('<a>','g'),"<span id=\"neurodna\" style=\"background-color:blue\"/>");
  stra=stra.replace(new RegExp('</a>','g'),"</span><br>");
}

function runTest() {
  for (var j = 1000; j <= 10000; j += 1000)
  {
    neurodna(j);
  }
}

function neurodna(limit) {
  var prepare="<go>";
  for(var i=0;i<limit;i++) {
    prepare += "<a>neurodna</a>";
  }
  prepare+="</go>";
  var da1=new Date();
  replace(prepare);
  var da2=new Date();
  data.X.push(limit);
  data.Y.push(da2-da1);
  gc();
}

runTest();

var order = BigO(data);

var msg = '';
for (var p = 0; p < data.X.length; p++)
{
  msg += '(' + data.X[p] + ', ' + data.Y[p] + '); ';
}
printStatus(msg);
printStatus('Order: ' + order);
reportCompare(true, order < 2, summary + ' BigO ' + order + ' < 2');

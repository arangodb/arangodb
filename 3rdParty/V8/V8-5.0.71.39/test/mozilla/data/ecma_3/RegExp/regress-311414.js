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
 * Contributor(s): timeless
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

var gTestfile = 'regress-311414.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 311414;
var summary = 'RegExp captured tail match should be O(N)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
function q1(n) {
  var c = [];
  c[n] = 1;
  c = c.join(" ");
  var d = Date.now();
  var e = c.match(/(.*)foo$/);
  var f = Date.now();
  return (f - d);
}

function q2(n) {
  var c = [];
  c[n] = 1;
  c = c.join(" ");
  var d = Date.now();
  var e = /foo$/.test(c) && c.match(/(.*)foo$/);
  var f = Date.now();
  return (f - d);
}

var data1 = {X:[], Y:[]};
var data2 = {X:[], Y:[]};

for (var x = 500; x < 5000; x += 500)
{
  var y1 = q1(x);
  var y2 = q2(x);
  data1.X.push(x);
  data1.Y.push(y1);
  data2.X.push(x);
  data2.Y.push(y2);
  gc();
}

var order1 = BigO(data1);
var order2 = BigO(data2);

var msg = '';
for (var p = 0; p < data1.X.length; p++)
{
  msg += '(' + data1.X[p] + ', ' + data1.Y[p] + '); ';
}
printStatus(msg);
printStatus('Order: ' + order1);
reportCompare(true, order1 < 2 , summary + ' BigO ' + order1 + ' < 2');

msg = '';
for (var p = 0; p < data2.X.length; p++)
{
  msg += '(' + data2.X[p] + ', ' + data2.Y[p] + '); ';
}
printStatus(msg);
printStatus('Order: ' + order2);
reportCompare(true, order2 < 2 , summary + ' BigO ' + order2 + ' < 2');

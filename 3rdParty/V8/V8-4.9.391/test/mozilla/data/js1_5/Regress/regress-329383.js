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
 * Contributor(s): Brendan Eich
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

var gTestfile = 'regress-329383.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 329383;
var summary = 'Math copysign issues';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var inputs = [
  -Infinity,
  -10.01,
  -9.01,
  -8.01,
  -7.01,
  -6.01,
  -5.01,
  -4.01,
  -Math.PI,
  -3.01,
  -2.01,
  -1.01,
  -0.5,
  -0.49,
  -0.01,
  -0,
  0,
  +0,
  0.01,
  0.49,
  0.50,
  0,
  1.01,
  2.01,
  3.01,
  Math.PI,
  4.01,
  5.01,
  6.01,
  7.01,
  8.01,
  9.01,
  10.01,
  Infinity
  ];

var iinput;

for (iinput = 0; iinput < inputs.length; iinput++)
{
  var input = inputs[iinput];
  reportCompare(Math.round(input),
                emulateRound(input),
                summary + ': Math.round(' + input + ')');
}

reportCompare(isNaN(Math.round(NaN)),
              isNaN(emulateRound(NaN)),
              summary + ': Math.round(' + input + ')');

function emulateRound(x)
{
  if (!isFinite(x) || x === 0) return x
    if (-0.5 <= x && x < 0) return -0
      return Math.floor(x + 0.5)
      }

var z;

z = Math.min(-0, 0);

reportCompare(-Math.PI, Math.atan2(z, z), summary + ': Math.atan2(-0, -0)');
reportCompare(-Infinity, 1/z, summary + ': 1/-0');

z = Math.max(-0, 0);

reportCompare(0, Math.atan2(z, z), summary + ': Math.atan2(0, 0)');
reportCompare(Infinity, 1/z, summary + ': 1/0');

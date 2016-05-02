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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Boris Zbarsky
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

var gTestfile = 'regress-456826.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 456826;
var summary = 'Do not assert with JIT during OOM';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  jit(true);

  if (typeof gcparam != 'undefined')
  {
    gcparam("maxBytes", 22000);
  }

  const numRows = 600;
  const numCols = 600;
  var scratch = {};
  var scratchZ = {};

  function complexMult(a, b) {
    var newr = a.r * b.r - a.i * b.i;
    var newi = a.r * b.i + a.i * b.r;
    scratch.r = newr;
    scratch.i = newi;
    return scratch;
  }

  function complexAdd(a, b) {
    scratch.r = a.r + b.r;
    scratch.i = a.i + b.i;
    return scratch;
  }

  function abs(a) {
    return Math.sqrt(a.r * a.r + a.i * a.i);
  }

  function computeEscapeSpeed(c) {
    scratchZ.r = scratchZ.i = 0;
    const scaler = 5;
    const threshold = (colors.length - 1) * scaler + 1;
    for (var i = 1; i < threshold; ++i) {
      scratchZ = complexAdd(c, complexMult(scratchZ, scratchZ));
      if (scratchZ.r * scratchZ.r + scratchZ.i * scratchZ.i > 4) {
        return Math.floor((i - 1) / scaler) + 1;
      }
    }
    return 0;
  }

  const colorStrings = [
    "black",
    "green",
    "blue",
    "red",
    "purple",
    "orange",
    "cyan",
    "yellow",
    "magenta",
    "brown",
    "pink",
    "chartreuse",
    "darkorange",
    "crimson",
    "gray",
    "deeppink",
    "firebrick",
    "lavender",
    "lawngreen",
    "lightsalmon",
    "lime",
    "goldenrod"
    ];

  var colors = [];

  function createMandelSet(realRange, imagRange) {
    var start = new Date();

    // Set up our colors
    for each (var color in colorStrings) {
        var [r, g, b] = [0, 0, 0];
        colors.push([r, g, b, 0xff]);
      }

    var realStep = (realRange.max - realRange.min)/numCols;
    var imagStep = (imagRange.min - imagRange.max)/numRows;
    for (var i = 0, curReal = realRange.min;
         i < numCols;
         ++i, curReal += realStep) {
      for (var j = 0, curImag = imagRange.max;
           j < numRows;
           ++j, curImag += imagStep) {
        var c = { r: curReal, i: curImag }
        var n = computeEscapeSpeed(c);
      }
    }
    print(Date.now() - start);
  }

  var realRange = { min: -2.1, max: 2 };
  var imagRange = { min: -2, max: 2 };
  createMandelSet(realRange, imagRange);

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

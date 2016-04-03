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
 * Contributor(s): Andreas Gal
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

var gTestfile = 'regress-451974-02.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 451974;
var summary = 'TM: loops with anon functions should not be slower with jit enabled';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var chars = '0123456789abcdef';
  var size = 1000;
  var mult = 100;
  var densearray = [];
  var lsize = size;

  while (lsize--) 
  { 
    densearray.push(chars); 
  }

  function loop()
  {
    var start = new Date();

    for (var a = 0; a < mult; a++) 
    {
      var f = (function(x){}); 
      for (var i = 0, len = densearray.length; i < len; i++) 
      { 
        f(densearray[i]); 
      }
    }

    var stop = new Date();
    return stop - start;
  }

  jit(false);
  var timenonjit = loop();
  jit(true);
  var timejit = loop();
  jit(false);

  print('time: nonjit = ' + timenonjit + ', jit = ' + timejit);

  expect = true;
  actual = timejit < timenonjit/2;

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

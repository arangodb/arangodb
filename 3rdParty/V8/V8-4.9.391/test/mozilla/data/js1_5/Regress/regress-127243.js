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
 * Contributor(s): Giscard Girard
 *                 Brendan Eich
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

var gTestfile = 'regress-127243.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 127243;
var summary = 'Do not crash on watch';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof window != 'undefined' && typeof document != 'undefined') 
{
  // delay test driver end
  gDelayTestDriverEnd = true;
  window.addEventListener('load', handleLoad, false);
}
else
{
  printStatus('This test must be run in the browser');
  reportCompare(expect, actual, summary);

}

var div;

function handleLoad()
{
  div = document.createElement('div');
  document.body.appendChild(div);
  div.setAttribute('id', 'id1');
  div.style.width = '50px';
  div.style.height = '100px';
  div.style.overflow = 'auto';

  for (var i = 0; i < 5; i++)
  {
    var p = document.createElement('p');
    var t = document.createTextNode('blah');
    p.appendChild(t);
    div.appendChild(p);
  }

  div.watch('scrollTop', wee);

  setTimeout('setScrollTop()', 1000);
}

function wee(id, oldval, newval)
{
  var t = document.createTextNode('setting ' + id +
                                  ' value ' + div.scrollTop +
                                  ' oldval ' + oldval +
                                  ' newval ' + newval);
  var p = document.createElement('p');
  p.appendChild(t);
  document.body.appendChild(p);

  return newval;
}

function setScrollTop()
{
  div.scrollTop = 42;

  reportCompare(expect, actual, summary);

  gDelayTestDriverEnd = false;
  jsTestDriverEnd();

}

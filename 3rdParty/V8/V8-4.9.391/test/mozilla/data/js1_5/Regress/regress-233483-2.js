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
 * Contributor(s): Alexander LAW <1@1o.ru>
 *                 WADA <m-wada@japan.com>
 *                 Bob Clary <bob@clary.com>
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

var gTestfile = 'regress-233483-2.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 233483;
var summary = 'Don\'t crash with null properties - Browser only';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof document == 'undefined')
{
  reportCompare(expect, actual, summary);
}
else
{ 
  // delay test driver end
  gDelayTestDriverEnd = true;

  actual = 'Crash';
  window.onload = onLoad;
}

function onLoad()
{
  var a = new Array();
  var pe;
  var x;
  var s;

  setform();

  for (pe=document.getElementById("test"); pe; pe=pe.parentNode)
  {
    a[a.length] = pe;
  }

  // can't document.write since this is in after load fires
  s = a.toString();

  actual = 'No Crash';

  reportCompare(expect, actual, summary);

  gDelayTestDriverEnd = false;
  jsTestDriverEnd();
}

function setform()
{
  var form  = document.body.appendChild(document.createElement('form'));
  var table = form.appendChild(document.createElement('table'));
  var tbody = table.appendChild(document.createElement('tbody'));
  var tr    = tbody.appendChild(document.createElement('tr'));
  var td    = tr.appendChild(document.createElement('td'))
    var input = td.appendChild(document.createElement('input'));

  input.setAttribute('id', 'test');
  input.setAttribute('value', '1232');

}

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
 * Contributor(s): Adrian Klein <dragosan@dragosan.net>
 *                 Martin Honnen <martin.honnen@gmx.de>
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

var gTestfile = 'regress-231518.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 231518;
var summary = 'decompiler must quote keywords and non-identifier property names';
var actual = '';
var expect = 'no error';
var status;
var object;
var result;

printBugNumber(BUGNUMBER);
printStatus (summary);


if (typeof uneval != 'undefined')
{
  status = inSection(1) + ' eval(uneval({"if": false}))';

  try
  {
    object = {'if': false };
    result = uneval(object);
    printStatus('uneval returns ' + result);
    eval(result);
    actual = 'no error';
  }
  catch(e)
  {
    actual = 'error';
  }
 
  reportCompare(expect, actual, status);

  status = inSection(2) + ' eval(uneval({"if": "then"}))';

  try
  {
    object = {'if': "then" };
    result = uneval(object);
    printStatus('uneval returns ' + result);
    eval(result);
    actual = 'no error';
  }
  catch(e)
  {
    actual = 'error';
  }
 
  reportCompare(expect, actual, status);

  status = inSection(3) + ' eval(uneval(f))';

  try
  {
    result = uneval(f);
    printStatus('uneval returns ' + result);
    eval(result);
    actual = 'no error';
  }
  catch(e)
  {
    actual = 'error';
  }
 
  reportCompare(expect, actual, status);

  status = inSection(2) + ' eval(uneval(g))';

  try
  {
    result = uneval(g);
    printStatus('uneval returns ' + result);
    eval(result);
    actual = 'no error';
  }
  catch(e)
  {
    actual = 'error';
  }
 
  reportCompare(expect, actual, status);
}

function f()
{
  var obj = new Object();

  obj['name']      = 'Just a name';
  obj['if']        = false;
  obj['some text'] = 'correct';
}

function g()
{
  return {'if': "then"};
}


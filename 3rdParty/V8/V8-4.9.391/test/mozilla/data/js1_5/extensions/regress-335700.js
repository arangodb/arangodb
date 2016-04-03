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
 * Contributor(s): Charles Verdon
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

var gTestfile = 'regress-335700.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 335700;
var summary = 'Object Construction with getter closures should be O(N)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
test('Object', Object);
test('ObjectWithFunction', ObjectWithFunction);
test('ObjectWithGetter', ObjectWithGetter);

function test(desc, ctor)
{
  var start = 00000;
  var stop  = 40000;
  var incr  = (stop - start)/10;

  desc = summary + ': ' + desc;

  var data = {X:[], Y:[]};
  for (var i = start; i <= stop; i += incr)
  { 
    data.X.push(i);
    data.Y.push(runStart(ctor, i));
    gc();
  }

  var order = BigO(data);

  var msg = '';
  for (var p = 0; p < data.X.length; p++)
  {
    msg += '(' + data.X[p] + ', ' + data.Y[p] + '); ';
  }

  print(msg);
 
  reportCompare(true, order < 2, desc + ': BigO ' + order + ' < 2');

}

function ObjectWithFunction()
{
  var m_var = null;
  this.init = function(p_var) {
    m_var = p_var;
  }
  this.getVar = function()
    {
      return m_var;
    }
  this.setVar = function(v)
    {
      m_var = v;
    }
}
function ObjectWithGetter()
{
  var m_var = null;
  this.init = function(p_var) {
    m_var = p_var;
  }
  this.Var getter = function() {
    return m_var;
  }
  this.Var setter = function(v) {
    m_var = v;
  }
}

function runStart(ctor, limit)
{
  var arr = [];
  var start = Date.now();

  for (var i=0; i < limit; i++) {
    var obj = new ctor();
    obj.Var = 42;
    arr.push(obj);
  }

  var end = Date.now();
  var diff = end - start;

  return diff;
}

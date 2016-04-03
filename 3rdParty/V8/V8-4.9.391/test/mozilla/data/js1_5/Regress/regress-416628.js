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
 * Portions created by the Initial Developer are Copyright (C) 2007
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

var gTestfile = 'regress-416628.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 416628;
var summary = 'O(n^2) blowup due to overlong cx->tempPool arena list';
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
 
  var data = {X:[], Y:[]};

  Function.prototype.inherits = function(parentCtor) {
    moo_inherits(this, parentCtor);
  };

  moo_inherits = function(childCtor, parentCtor) {
    /** @constructor */
    function tempCtor() {};
    tempCtor.prototype = parentCtor.prototype;
    childCtor.superClass_ = parentCtor.prototype;
    childCtor.prototype = new tempCtor();
    childCtor.prototype.constructor = childCtor;
  };

  var jstart =  100;
  var jstop  = 1000;
  var jinterval = (jstop - jstart)/9;

  if (true) { 
    for (var j = jstart; j < jstop; j += jinterval)
    {
      data.X.push(j);
      var code = '';
      for (var i = 0; i < j; i++)
      {
        code += createCode(i);
      }
      gc();
      var start = new Date();
      eval(code);
      var stop = new Date();
      data.Y.push(stop - start);
    }
  }

  var order = BigO(data);

  var msg = '';
  for (var p = 0; p < data.X.length; p++)
  {
    msg += '(' + data.X[p] + ', ' + data.Y[p] + '); ';
  }
  printStatus(msg);
  printStatus('Order: ' + order);

  reportCompare(true, order < 2, 'BigO ' + order + ' < 2');

  exitFunc ('test');
}

function createCode(i)
{
  var code = '';

  code += "var str1_" + i + "='This is 1 a test " + i + " string.';";
  code += "var str2_" + i + "='This is 2 a test " + i + " string.';";
  code += "var str3_" + i + "='This is 3 a test " + i + " string.';";
  code += "var str4_" + i + "='This is 4 a test " + i + " string.';";
  code += "var str5_" + i + "='This is 5 a test " + i + " string.';";
  code += "var str6_" + i + "='This is 6 a test " + i + " string.';";
  code += "var str7_" + i + "='This is 7 a test " + i + " string.';";
  code += "";
  code += "var base" + i + " = function() {this.a_=4;this.b_=5};";
  code += "base" + i + ".f1 = function() {this.a_=4;this.b_=5};";
  code += "base" + i + ".prototype.f2 = function() {this.a_=4;this.b_=5};";
  code += "base" + i + ".prototype.f3 = function() {this.a_=4;this.b_=5};";
  code += "base" + i + ".prototype.f4 = function() {this.a_=4;this.b_=5};";
  code += "base" + i + ".prototype.f5 = function() {this.a_=4;this.b_=5};";
  code += "";
  code += "var child" + i + " = function() {this.a_=4;this.b_=5};";
  code += "child" + i + ".inherits(base" + i + ");";
  code += "child" + i + ".f1 = function() {this.a_=4;this.b_=5};";
  code += "child" + i + ".prototype.f2 = function() {this.a_=4;this.b_=5};";
  code += "child" + i + ".prototype.f3 = function() {this.a_=4;this.b_=5};";
  code += "child" + i + ".prototype.f4 = function() {this.a_=4;this.b_=5};";
  code += "child" + i + ".prototype.f5 = function() {this.a_=4;this.b_=5};";
  code += "";
  code += "var gchild" + i + " = function() {this.a_=4;this.b_=5};";
  code += "gchild" + i + ".inherits(child" + i + ");";
  code += "gchild" + i + ".f1 = function() {this.a_=4;this.b_=5};";
  code += "gchild" + i + ".prototype.f2 = function() {this.a_=4;this.b_=5};";
  code += "gchild" + i + ".prototype.f3 = function() {this.a_=4;this.b_=5};";
  code += "gchild" + i + ".prototype.f4 = function() {this.a_=4;this.b_=5};";
  code += "gchild" + i + ".prototype.f5 = function() {this.a_=4;this.b_=5};";

  return code;
}

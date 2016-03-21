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
 * Jeff Walden.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Norris Boyd
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

// See http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Guide:Iterators_and_Generators

var gTestfile = 'iterator-ctor.js';
//-----------------------------------------------------------------------------
var BUGNUMBER  = "410725";
var summary = "Test of the global Iterator constructor";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function iteratorToArray(iterator) {
  var result = [];
  for (var i in iterator) {
    result[result.length] = i;
  }
  return result.sort();
}

var obj = {a:1, b:2};

reportCompare('["a", "b"]',
              uneval(iteratorToArray(obj)),
              'uneval(iteratorToArray(obj))');

reportCompare('[["a", 1], ["b", 2]]',
              uneval(iteratorToArray(Iterator(obj))),
              'uneval(iteratorToArray(Iterator(obj)))');

reportCompare('[["a", 1], ["b", 2]]',
              uneval(iteratorToArray(new Iterator(obj))),
              'uneval(iteratorToArray(new Iterator(obj)))');

reportCompare('[["a", 1], ["b", 2]]',
              uneval(iteratorToArray(Iterator(obj,false))),
              'uneval(iteratorToArray(Iterator(obj,false)))');

reportCompare('[["a", 1], ["b", 2]]',
              uneval(iteratorToArray(new Iterator(obj,false))),
              'uneval(iteratorToArray(new Iterator(obj,false)))');

reportCompare('["a", "b"]',
              uneval(iteratorToArray(Iterator(obj,true))),
              'uneval(iteratorToArray(Iterator(obj,true)))');

reportCompare('["a", "b"]',
              uneval(iteratorToArray(new Iterator(obj,true))),
              'uneval(iteratorToArray(new Iterator(obj,true)))');

var flag;
var obji = {a:1, b:2};
obji.__iterator__ = function (b) { flag = b; yield -1; yield -2; }

flag = -1;
reportCompare('[-1, -2]',
              uneval(iteratorToArray(obji)),
              'uneval(iteratorToArray(obji))');
reportCompare(true, flag, 'uneval(iteratorToArray(obji)) flag');

flag = -1;
reportCompare('[-1, -2]',
              uneval(iteratorToArray(Iterator(obji))),
              'uneval(iteratorToArray(Iterator(obji)))');
reportCompare(false, flag, 'uneval(iteratorToArray(Iterator(obji))) flag');

flag = -1;
reportCompare('[["__iterator__", (function (b) {flag = b;yield -1;yield -2;})], ["a", 1], ["b", 2]]',
              uneval(iteratorToArray(new Iterator(obji))),
              'uneval(iteratorToArray(new Iterator(obji)))');
reportCompare(-1, flag, 'uneval(iteratorToArray(new Iterator(obji))) flag');

flag = -1;
reportCompare('[-1, -2]',
              uneval(iteratorToArray(Iterator(obji,false))),
              'uneval(iteratorToArray(Iterator(obji,false)))');
reportCompare(false, flag, 'uneval(iteratorToArray(Iterator(obji,false))) flag');

flag = -1;
reportCompare('[["__iterator__", (function (b) {flag = b;yield -1;yield -2;})], ["a", 1], ["b", 2]]',
              uneval(iteratorToArray(new Iterator(obji,false))),
              'uneval(iteratorToArray(new Iterator(obji,false)))');
reportCompare(-1, flag, 'uneval(iteratorToArray(new Iterator(obji,false))) flag');

flag = -1;
reportCompare('[-1, -2]',
              uneval(iteratorToArray(Iterator(obji,true))),
              'uneval(iteratorToArray(Iterator(obji,true)))');
reportCompare(true, flag, 'uneval(iteratorToArray(Iterator(obji,true))) flag');

flag = -1;
reportCompare('["__iterator__", "a", "b"]',
              uneval(iteratorToArray(new Iterator(obji,true))),
              'uneval(iteratorToArray(new Iterator(obji,true)))');
reportCompare(-1, flag, 'uneval(iteratorToArray(new Iterator(obji,true))) flag');

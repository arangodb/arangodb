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
 * Contributor(s): Jason Barnabe (np)
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

var gTestfile = 'regress-320119.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 320119;
var summary = 'delegating objects and arguments, arity, caller, name';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

printStatus('original test');

function origtest(name, bar)
{
  this.name = name;
  this.bar = bar;
}

function Monty(id, name, bar)
{
  this.id = id;
  this.base = origtest;
  this.base(name, bar);
}

Monty.prototype = origtest;

function testtwo(notNamedName, bar)
{
  this.name = notNamedName;
  this.bar = bar;
}

function Python(id, notNamedName, bar)
{
  this.id = id;
  this.base = origtest;
  this.base(notNamedName, bar);
}

Python.prototype = testtwo;

var foo = new Monty(1, 'my name', 'sna!');

var manchu = new Python(1, 'my name', 'sna!');

printStatus('foo.name: ' + foo.name);
printStatus('manchu.name: ' + manchu.name);

expect = 'my name:my name';
actual = foo.name + ':' + manchu.name;
reportCompare(expect, actual, summary + ': override function..name');

// end original test

printStatus('test shared properties');

function testshared()
{
}

expect = false;
actual = testshared.hasOwnProperty('arguments');
reportCompare(expect, actual, summary + ': arguments no longer shared');

expect = false;
actual = testshared.hasOwnProperty('caller');
reportCompare(expect, actual, summary + ': caller no longer shared');

expect = false;
actual = testshared.hasOwnProperty('arity');
reportCompare(expect, actual, summary + ': arity no longer shared');

expect = false;
actual = testshared.hasOwnProperty('name');
reportCompare(expect, actual, summary + ': name no longer shared');

expect = true;
actual = testshared.hasOwnProperty('length');
reportCompare(expect, actual, summary + ': length still shared');

printStatus('test overrides');

function Parent()
{
  this.arguments = 'oarguments';
  this.caller = 'ocaller';
  this.arity = 'oarity';
  this.length = 'olength';
  this.name = 'oname';
}

function Child()
{
  this.base = Parent;
  this.base();
}

Child.prototype = Parent;

Child.prototype.value = function()
{
  return this.arguments + ',' + this.caller + ',' + this.arity + ',' +
  this.length + ',' + this.name;
};

var child = new Child();

expect = 'oarguments,ocaller,oarity,0,oname';
actual = child.value();
reportCompare(expect, actual, summary + ': overrides');

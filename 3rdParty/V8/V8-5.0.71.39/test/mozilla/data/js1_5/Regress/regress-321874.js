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
 * Contributor(s): Yuh-Ruey Chen
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

var gTestfile = 'regress-321874.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 321874;
var summary = 'lhs must be a reference in (for lhs in rhs) ...';
var actual = '';
var expect = '';
var section;

printBugNumber(BUGNUMBER);
printStatus (summary);

function a() {}
var b = {foo: 'bar'};

printStatus('for-in tests');

var v;
section = summary + ': for((v) in b);';
expect = 'foo';
printStatus(section);
try
{
  eval('for ((v) in b);');
  actual = v;
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, section);

section = summary + ': function foo(){for((v) in b);};foo();';
expect = 'foo';
printStatus(section);
try
{
  eval('function foo(){ for ((v) in b);}; foo();');
  actual = v;
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, section);

section = summary + ': for(a() in b);';
expect = 'error';
printStatus(section);
try
{
  eval('for (a() in b);');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, section);

section = summary + ': function foo(){for(a() in b);};foo();';
expect = 'error';
printStatus(section);
try
{
  eval('function foo(){ for (a() in b);};foo();');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, section);

section = ': for(new a() in b);';
expect = 'error';
printStatus(section);
try
{
  eval('for (new a() in b);');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

section = ': function foo(){for(new a() in b);};foo();';
expect = 'error';
printStatus(section);
try
{
  eval('function foo(){ for (new a() in b);};foo();');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

section = ': for(void in b);';
expect = 'error';
printStatus(section);
try
{
  eval('for (void in b);');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

section = ': function foo(){for(void in b);};foo();';
expect = 'error';
printStatus(section);
try
{
  eval('function foo(){ for (void in b);};foo();');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

var d = 1;
var e = 2;
expect = 'error';
section = ': for((d*e) in b);';
printStatus(section);
try
{
  eval('for ((d*e) in b);');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

var d = 1;
var e = 2;
expect = 'error';
section = ': function foo(){for((d*e) in b);};foo();';
printStatus(section);
try
{
  eval('function foo(){ for ((d*e) in b);};foo();');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

const c = 0;
expect = 0;
section = ': for(c in b);';
printStatus(section);
try
{
  eval('for (c in b);');
  actual = c;
  printStatus('typeof c: ' + (typeof c) + ', c: ' + c);
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

expect = 0;
section = ': function foo(){for(c in b);};foo();';
printStatus(section);
try
{
  eval('function foo(){ for (c in b);};foo();');
  actual = c;
  printStatus('typeof c: ' + (typeof c) + ', c: ' + c);
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

if (typeof it != 'undefined')
{
  printStatus('Shell tests: it.item() can return a reference type');

  expect = 'foo';
  section = ': for(it.item(0) in b);';
  printStatus(section);
  try
  {
    eval('for (it.item(0) in b);');
    actual = it.item(0);
  }
  catch(ex)
  {
    printStatus(ex+'');
    actual = 'error';
  } 
  reportCompare(expect, actual, summary + section);

  expect = 'foo';
  section = ': function foo(){for(it.item(0) in b);};foo();';
  printStatus(section);
  try
  {
    eval('function foo(){ for (it.item(0) in b);};foo();');
    actual = it.item(0);
  }
  catch(ex)
  {
    printStatus(ex+'');
    actual = 'error';
  } 
  reportCompare(expect, actual, summary + section);
}

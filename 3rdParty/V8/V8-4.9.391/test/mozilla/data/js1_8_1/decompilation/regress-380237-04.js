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
 * Contributor(s): Jesse Ruderman
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

var gTestfile = 'regress-380237-04.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 380237;
var summary = 'Generator expressions parenthesization test';
var actual = '';
var expect = '';


/*

Given that parentheization seems so fragile *and* the rules for where
genexps are allowed keep changing, I thought it would be good to have
a way to test that:

1) unparenthesized genexps are allowed in some places and the
decompilation is sane and not over-parenthesized

2) unparenthesized genexps are disallowed in many places and when
there are parens, the decompilation is sane and not over-parenthesized

*/

// |genexp| must have the exact same whitespace the decompiler uses
genexp = "x * x for (x in [])";
genexpParened = "(" + genexp + ")";
genexpParenedTwice = "(" + genexpParened + ")";

// Warning: be careful not to put [] around stuff, because that would
// cause it to be treated as an array comprehension instead of a
// generator expression!

// Statements
doesNotNeedParens(1, "if (xx) { }");
needParens(2, "if (1, xx) { }");
needParens(3, "if (xx, 1) { }");
doesNotNeedParens(4, "do { } while (xx);");
doesNotNeedParens(5, "while (xx) { }");
doesNotNeedParens(6, "switch (xx) { }");
doesNotNeedParens(7, "with (xx) { }");
needParens(8, "switch (x) { case xx: }");
needParens(9, "return xx;");
needParens(10, "yield xx;");
needParens(11, "for (xx;;) { }");
needParens(12, "for (;xx;) { }", "function anonymous() {\n    for (;;) {\n    }\n}");
needParens(13, "for (;;xx) { }");
needParens(14, "for (i in xx) { }");
needParens(15, "throw xx");
needParens(16, "try { } catch (e if xx) { }");
needParens(17, "let (x=3) xx");
needParens(18, "let (x=xx) 3");

// Function calls
doesNotNeedParens(19, "f(xx);");
needParens(20, "f(xx, 1);");
needParens(21, "f(1, xx);");
doesNotNeedParens(22, "/x/(xx);");
needParens(23, "/x/(xx, 1);");
needParens(24, "/x/(1, xx);");

// eval is special and often confuses the decompiler.
doesNotNeedParens(25, "eval(xx);");
needParens(26, "eval(xx, 1);");
needParens(27, "eval(1, xx);");

// Expressions
needParens(28, "xx;");            // ???
needParens(29, "var g = xx;");    // ???
needParens(30, "g += xx;");
needParens(31, "xx();");
needParens(32, "xx() = 3;");
needParens(33, "a ? xx : c");
needParens(34, "xx ? b : c");
needParens(35, "a ? b : xx");
needParens(36, "1 ? xx : c");
needParens(37, "0 ? b : xx");
needParens(38, "1 + xx");
needParens(39, "xx + 1");
needParens(40, "1, xx");
doesNotNeedParens(41, "+(xx)");
doesNotNeedParens(42, "!(xx)");
needParens(43, "xx, 1");
needParens(44, "[1, xx]");
needParens(45, "[xx, 1]");
needParens(46, "[#1=xx,3]");
needParens(47, "[#1=xx,#1#]");
needParens(48, "xx.p");
needParens(49, "xx.@p");
needParens(50, "typeof xx;");
needParens(51, "void xx;");
needParens(52, "({ a: xx })");
needParens(53, "({ a: 1, b: xx })");
needParens(54, "({ a: xx, b: 1 })");
needParens(55, "({ a getter: xx })");
needParens(56, "<x a={xx}/>");
doesNotNeedParens(57, "new (xx);");
doesNotNeedParens(58, "new a(xx);");


// Generator expressions cannot be used as LHS, even though they're syntactic 
// sugar for something that looks a lot like an "lvalue return": (f() = 3).

rejectLHS(59, "++ (xx);");
rejectLHS(60, "delete xx;");
rejectLHS(61, "delete (xx);");
rejectLHS(62, "for (xx in []) { }");
rejectLHS(63, "for ((xx) in []) { }");
rejectLHS(64, "try { } catch(xx) { }");
rejectLHS(65, "try { } catch([(xx)]) { }");
rejectLHS(66, "xx += 3;");
rejectLHS(67, "(xx) += 3;");
rejectLHS(68, "xx = 3;");

// Assignment
rejectLHS(69, "        (xx) = 3;");
rejectLHS(70, "var     (xx) = 3;");
rejectLHS(71, "const   (xx) = 3;");
rejectLHS(72, "let     (xx) = 3;");

// Destructuring assignment
rejectLHS(73, "        [(xx)] = 3;");
rejectLHS(74, "var     [(xx)] = 3;");
rejectLHS(75, "const   [(xx)] = 3;");
rejectLHS(76, "let     [(xx)] = 3;");

// Group assignment (Spidermonkey optimization for certain
// destructuring assignments)
rejectLHS(77, "        [(xx)] = [3];");
rejectLHS(78, "var     [(xx)] = [3];");
rejectLHS(79, "const   [(xx)] = [3];");
rejectLHS(80, "let     [(xx)] = [3];");

// Destructuring & group assignment for array comprehensions, just for kicks.
rejectLHS(81, "        [xx] = [3];");
rejectLHS(82, "var     [xx] = [3];");
rejectLHS(83, "const   [xx] = [3];");
rejectLHS(84, "let     [xx] = 3;");
rejectLHS(85, "        [xx] = 3;");
rejectLHS(86, "var     [xx] = 3;");
rejectLHS(87, "const   [xx] = 3;");
rejectLHS(88, "let     [xx] = 3;");

// This is crazy, ambiguous, and/or buggy.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=380237#c23 et seq.
//doesNotNeedParens("(yield xx);");

print("Done!");

function doesNotNeedParens(section, pat)
{
  print("Testing section " + section + " pattern " + pat);

  var f, ft;
  sanityCheck(section, pat);

  expect = 'No Error';
  actual = '';
  ft = pat.replace(/xx/, genexp);
    try {
      f = new Function(ft);
      actual = 'No Error';
    } catch(e) {
      print("Unparenthesized genexp SHOULD have been accepted here!");
      actual = e + '';
    }
  reportCompare(expect, actual, summary + ': doesNotNeedParens section ' + section + ' pattern ' + pat);

  roundTripTest(section, f);

  // Make sure the decompilation is not over-parenthesized.
  var uf = "" + f;
  if (pat.indexOf("(xx)") != -1)
    overParenTest(section, f);
  //  else
  //    print("Skipping the over-parenthesization test, because I don't know how to test for over-parenthesization when the pattern doesn't have parens snugly around it.")
}

function needParens(section, pat, exp)
{
  print("Testing section " + section + " pattern " + pat);

  var f, ft;
  sanityCheck(section, pat);

  expect = 'SyntaxError';
  actual = '';
  ft = pat.replace(/xx/, genexp);
  try {
    f = new Function(ft);
    print("Unparenthesized genexp should NOT have been accepted here!");
  } catch(e) { 
    /* expected to throw */ 
    actual = e.name;
  }
  reportCompare(expect, actual, summary + ': needParens section ' + section + ' pattern ' + pat);

  expect = 'No Error';
  actual = '';
  ft = pat.replace(/xx/, genexpParened);
  try {
    f = new Function(ft);
    actual = 'No Error';
  } catch(e) { 
    print("Yikes!"); 
    actual = e + '';
  }
  reportCompare(expect, actual, summary + ': needParens section ' + section + ' ft ' + ft);

  roundTripTest(section, f, exp);
  overParenTest(section, f, exp);
}

function rejectLHS(section, pat)
{
  print("Testing section " + section + " pattern " + pat);

  // sanityCheck(pat); // because 'z' should be accepted as an LHS or binding
    
  var ft;
    
  expect = 'SyntaxError';
  actual = '';
  ft = pat.replace(/xx/, genexp)
    try {
      new Function(ft);
      print("That should have been a syntax error!");
      actual = 'No Error';
    } catch(e) { 
      actual = e.name;
    }
  reportCompare(expect, actual, summary + ': rejectLHS section ' + section);
}


function overParenTest(section, f, exp)
{
  var uf = "" + f;
  if (uf == exp)
    return;

  if (uf.indexOf(genexp) == -1) {
    print('openParenTest: section ' + section + ' expected ' + exp + ' uf ' + uf + ' genexp optimized away');
    return;
  }

  reportCompare(false, uf.indexOf(genexpParened) == -1, summary + 
                ': overParenTest genexp snugly in parentheses: section ' + section + ' uf ' + uf);

  if (uf.indexOf(genexpParened) != -1) {
    reportCompare(true, uf.indexOf(genexpParenedTwice) == -1, summary + 
      ': overParensTest decompilation should not be over-parenthesized: section ' + ' uf ' + uf);
  }
}

function sanityCheck(section, pat)
{
  expect = '';
  actual = '';

  if (pat.indexOf("xx") == -1)
  {
    actual += "No 'xx' in this pattern? ";
  }

  var f, ft;
  ft = pat.replace(/xx/, "z");
  try {
    f = new Function(ft);
  } catch(e) { 
    actual += "Yowzers! Probably a bogus test!";
  }
  reportCompare(expect, actual, summary + ': sanityCheck section ' + section + ' pattern ' + pat);
}

function roundTripTest(section, f, exp)
{
  // Decompile
  var uf = "" + f;

  // Recompile
  expect = 'No Error';
  actual = '';
  var euf;
  try {
    euf = eval("(" + uf + ")");
    actual = 'No Error';
    reportCompare(expect, actual, summary + ': roundTripTest: section ' + section + ' uf ' + uf);
  } catch(e) {
    actual = e + '';
    reportCompare(expect, actual, summary + ': roundTripTest: section ' + section + ' uf ' + uf);
    return;
  }

  // Decompile again and make sure the decompilations match exactly.
  expect = exp || uf;
  actual = "" + euf;
  reportCompare(expect, actual, summary + ': roundTripTest no round-trip change: section ' + section);
}

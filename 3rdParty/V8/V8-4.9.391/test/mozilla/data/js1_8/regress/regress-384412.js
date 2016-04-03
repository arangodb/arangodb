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
 * Contributor(s): Joachim Kuebart
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

var gTestfile = 'regress-384412.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 384412;
var summary = 'Exercise frame handling code';
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
 
/*
 * Generators
 */

/* Generator yields properly */
  f = (function(n) { for (var i = 0; i != n; i++) yield i });
  g = f(3);
  expect(0, g.next());
  expect(1, g.next());
  expect(2, g.next());
  s = "no exception";
  try { g.next(); } catch (e) { s = e + ""; }
  expect("[object StopIteration]", s);

/* Generator yields properly in finally */
  f = (function(n) {
      try {
        for (var i = 0; i != n; i++) 
          yield i;
      } finally {
        yield "finally";
      }
    });

  g = f(3);
  expect(0, g.next());
  expect(1, g.next());
  expect(2, g.next());
  expect("finally", g.next());

/* Generator throws when closed with yield in finally */
  g = f(3);
  expect(0, g.next());
  s = "no exception";
  try { g.close(); } catch (e) { s = e + ""; };
  expect("TypeError: yield from closing generator " + f.toSource(), s);


/*
 * XML predicates
 */
  t = <xml><eins><name>ich</name></eins><eins><name>joki</name></eins></xml>;

/* Predicates, nested predicates and empty lists */
  expect(<eins><name>joki</name></eins>, t.eins.(name == "joki"));
  expect(t.eins, t.eins.(t.eins.(true)));
  expect(t.(false), t.eins.(false).(true));

/* Predicate with yield throws */
  f = (function() { t.eins.(yield true); });
  g = f();
  s = "no exception";
  try { g.next(); } catch (e) { s = e + ""; }
  expect("no exception", s);

/* Function with predicate without return returns void */
  f = (function() { t.eins.(true); });
  expect(undefined, f());

/* XML filter predicate in finally preserves return value */
  f = (function() {
      try {
        return "hallo";
      } finally {
        t.eins.(true);
      }
    });
  expect("hallo", f());


/*
 * Calls that have been replaced with js_PushFrame() &c...
 */
  f = (function() { return arguments[(arguments.length - 1) / 2]; });
  expect(2, f(1, 2, 3));
  expect(2, f.call(null, 1, 2, 3));
  expect(2, f.apply(null, [1, 2, 3]));
  expect("a1c", "abc".replace("b", f));
  s = "no exception";
  try {
    "abc".replace("b", (function() { throw "hello" }));
  } catch (e) {
    s = e + "";
  }
  expect("hello", s);
  expect(6, [1, 2, 3].reduce(function(a, b) { return a + b; }));
  s = "no exception";
  try {
    [1, 2, 3].reduce(function(a, b) { if (b == 2) throw "hello"; });
  } catch (e) {
    s = e + "";
  }
  expect("hello", s);

/*
 * __noSuchMethod__
 */
  o = {};
  s = "no exception";
  try {
    o.hello();
  } catch (e) {
    s = e + "";
  }
  expect("TypeError: o.hello is not a function", s);
  o.__noSuchMethod__ = (function() { return "world"; });
  expect("world", o.hello());
  o.__noSuchMethod__ = 1;
  s = "no exception";
  try {
    o.hello();
  } catch (e) {
    s = e + "";
  }
  expect("TypeError: o.hello is not a function", s);
  o.__noSuchMethod__ = {};
  s = "no exception";
  try {
    o.hello();
  } catch (e) {
    s = e + "";
  }
  expect("TypeError: o.hello() is not a function", s);
  s = "no exception";
  try {
    eval("o.hello()");
  } catch (e) {
    s = e + "";
  }
  expect("TypeError: o.hello() is not a function", s);
  s = "no exception";
  try { [2, 3, 0].sort({}); } catch (e) { s = e + ""; }
  expect("TypeError: [2, 3, 0].sort({}) is not a function", s);

/*
 * Generator expressions.
 */
  String.prototype.__iterator__ = (function () {
      /*
       * NOTE:
       * Without the "0 + ", the loop over <x/> does not terminate because
       * the iterator gets run on a string with an empty length property.
       */
      for (let i = 0; i != 0 + this.length; i++)
        yield this[i];
    });
  expect(["a1", "a2", "a3", "b1", "b2", "b3", "c1", "c2", "c3"] + "",
         ([a + b for (a in 'abc') for (b in '123')]) + "");
  expect("", ([x for (x in <x/>)]) + "");

/*
 * Version switching
 */
  if (typeof version == 'function')
  {
    var v = version(150);
    f = new Function("return version(arguments[0])");
    version(v);
    expect(150, f());
    expect(150, eval("f()"));
    expect(0, eval("f(0); f()"));
    version(v);
  }
  print("End of Tests");

/*
 * Utility functions
 */
  function expect(a, b) {
    print('expect: ' + a + ', actual: ' + b);
    reportCompare(a, b, summary);
  }


  exitFunc ('test');
}

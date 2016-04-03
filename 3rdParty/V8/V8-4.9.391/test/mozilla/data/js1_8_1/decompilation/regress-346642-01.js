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
 * Contributor(s): Jesse Ruderman
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

var gTestfile = 'regress-346642-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 346642;
var summary = 'decompilation of destructuring assignment';
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
 
  var f;

  f = function () {({a:{c:x}, b:x}) = ({b:3})}
  expect = 'function () {({a:{c:x}, b:x} = {b:3});}';
  actual = f + '';
  compareSource(expect, actual, summary + ': 1');

  f = function() { for(; let (x=3,y=4) null;) { } }
  expect = 'function() { for(; let (x=3,y=4) null;) { } }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 2');

  f = function() { let (x=3,y=4) {  } x = 5; }
  expect = 'function() { let (x=3,y=4) {  } x = 5; }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 3');

  f = function () { for ([11].x;;) break; }
  expect = 'function () { for ([11].x;;) { break;} }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 4');

  f = function() { new ({ a: b } = c) }
  expect = 'function() { new ({ a: b } = c); }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 5');

  f = function () { (let (x) 3)(); }
  expect = 'function () { (let (x) 3)(); }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 6');

  f = function () { (let (x) 3).foo(); }
  expect = 'function () { (let (x) 3).foo(); }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 7');

  f = function () { ({x: a(b)}) = window; }
  expect = 'function () { ({x: a(b)} = window); }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 8');

  f = function() { let ([a]=x) { } }
  expect = 'function() { let ([a]=x) { } }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 9');

  f = function( ){ new ([x] = k) }
  expect = 'function( ){ new ([x] = k); }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 10');

  f = function() { [a] = [b] = c }
  expect = 'function() { [a] = [b] = c; }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 11');

  f = function() { [] = 3 }
  expect = 'function() { [] = 3; }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 12');

  f = function() { ({}) = 3 }  
  expect = 'function() { [] = 3; }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 13');

  f = function () { while( {} = e ) ; }
  expect = 'function () { while( ( [] = e ) ) {} }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 14');

  f = function () { while( {} = (a)(b) ) ; }
  expect = 'function () { while( ( [] = a(b) ) ) {} }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 15');

  f = function (){[] = [a,b,c]}
  expect = 'function (){[] = [a,b,c];}';
  actual = f + '';
  compareSource(expect, actual, summary + ': 16');

  f = function (){for([] = [a,b,c];;);}
  expect = 'function (){for([] = [a,b,c];;){}}';
  actual = f + '';
  compareSource(expect, actual, summary + ': 17');

  f = function (){for(;;[] = [a,b,c]);}
  expect = 'function (){for(;;[] = [a,b,c]){}}';
  actual = f + '';
  compareSource(expect, actual, summary + ': 18');

  f = function (){for(let [] = [a,b,c];;);}
  expect = 'function (){for(let [] = [a,b,c];;){}}';
  actual = f + '';
  compareSource(expect, actual, summary + ': 19');

  f = function() { for (;; [x] = [1]) { } }
  expect = 'function() { for (;; [x] = [1]) { } } ';
  actual = f + '';
  compareSource(expect, actual, summary + ': 20');

  f = function() { [g['//']] = h }
  expect = 'function() { [g["//"]] = h; }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 21');

  f = (function() { for ( let [a,b]=[c,d] in [3]) { } })
    expect = 'function() { [c, d]; for ( let [a,b] in [3]) { } }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 22');

  f = function () { while(1) [a] = [b]; }
  expect = 'function () { while(true) {[a] = [b];} } ';
  actual = f + '';
  compareSource(expect, actual, summary + ': 23');

  f = function () { for(var [x, y] = r in p) { } }
  expect = 'function () { var [x, y] = r; for( [x, y] in p) { } }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 24');

  f = function() { for([x] = [];;) { } }
  expect = 'function() { for([x] = [];;) { } }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 25');

  f = function () { let ([y] = delete [1]) { } }
  expect = 'function () { let ([y] = ([1], true)) { } }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 26');

  f = function () { delete 4..x }
  expect = 'function () { delete (4).x; }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 27');

  f = function() { return [({ x: y }) = p for (z in 5)] }
  expect = 'function() { return [{ x: y } = p for (z in 5)]; }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 28');

  exitFunc ('test');
}

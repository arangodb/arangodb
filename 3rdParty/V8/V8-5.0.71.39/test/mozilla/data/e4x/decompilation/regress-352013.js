/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

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

gTestfile = 'regress-352013.js';

var BUGNUMBER = 352013;
var summary = 'Decompilation with new operator redeaux';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var l, m, r;
var nTests = 0;

// ---------------------------------------------------------------

l = function () { (new x(y))[z]; };
expect = 'function () { (new x(y))[z]; }';
actual = l + '';
compareSource(expect, actual, inSection(++nTests) + summary);

m = function () { new (x(y))[z]; };
expect = 'function () { new (x(y)[z]); }';
actual = m + '';
compareSource(expect, actual, inSection(++nTests) + summary);

r = function () { new (x(y)[z]); };
expect = 'function () { new (x(y)[z]); }';
actual = r + '';
compareSource(expect, actual, inSection(++nTests) + summary);

// ---------------------------------------------------------------

l = function () { (new x(y)).@a; };
expect = 'function () { (new x(y)).@a; }';
actual = l + '';
compareSource(expect, actual, inSection(++nTests) + summary);

m = function () { new (x(y)).@a; };
expect = 'function () { new (x(y).@a); }';
actual = m + '';
compareSource(expect, actual, inSection(++nTests) + summary);

r = function () { new (x(y).@a); };
expect = 'function () { new (x(y).@a); }';
actual = r + '';
compareSource(expect, actual, inSection(++nTests) + summary);

// ---------------------------------------------------------------

l = function () { (new x(y)).@n::a; };
expect = 'function () { (new x(y)).@[n::a]; }';
actual = l + '';
compareSource(expect, actual, inSection(++nTests) + summary);

m = function () { new (x(y)).@n::a; };
expect = 'function () { new (x(y).@[n::a]); }';
actual = m + '';
compareSource(expect, actual, inSection(++nTests) + summary);

r = function () { new (x(y).@n::a); };
expect = 'function () { new (x(y).@[n::a]); }';
actual = r + '';
compareSource(expect, actual, inSection(++nTests) + summary);

// ---------------------------------------------------------------

l = function () { (new x(y)).n::z; };
expect = 'function () { (new x(y)).n::z; }';
actual = l + '';
compareSource(expect, actual, inSection(++nTests) + summary);

m = function () { new (x(y)).n::z; };
expect = 'function () { new (x(y).n::z); }';
actual = m + '';
compareSource(expect, actual, inSection(++nTests) + summary);

r = function () { new (x(y).n::z); };
expect = 'function () { new (x(y).n::z); }';
actual = r + '';
compareSource(expect, actual, inSection(++nTests) + summary);

// ---------------------------------------------------------------

l = function () { (new x(y)).n::[z]; };
expect = 'function () { (new x(y)).n::[z]; }';
actual = l + '';
compareSource(expect, actual, inSection(++nTests) + summary);

m = function () { new (x(y)).n::[z]; };
expect = 'function () { new (x(y).n::[z]); }';
actual = m + '';
compareSource(expect, actual, inSection(++nTests) + summary);

r = function () { new (x(y).n::[z]); };
expect = 'function () { new (x(y).n::[z]); }';
actual = r + '';
compareSource(expect, actual, inSection(++nTests) + summary);

END();

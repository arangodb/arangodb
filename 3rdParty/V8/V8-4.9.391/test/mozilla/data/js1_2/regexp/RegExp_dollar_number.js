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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

gTestfile = 'RegExp_dollar_number.js';

/**
   Filename:     RegExp_dollar_number.js
   Description:  'Tests RegExps $1, ..., $9 properties'

   Author:       Nick Lerissa
   Date:         March 12, 1998
*/

var SECTION = 'As described in Netscape doc "What\'s new in JavaScript 1.2"';
var VERSION = 'no version';
var TITLE   = 'RegExp: $1, ..., $9';
var BUGNUMBER="123802";

startTest();
writeHeaderToLog('Executing script: RegExp_dollar_number.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$1
'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/);
new TestCase ( SECTION, "'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$1",
	       'abcdefghi', RegExp.$1);

// 'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$2
new TestCase ( SECTION, "'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$2",
	       'bcdefgh', RegExp.$2);

// 'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$3
new TestCase ( SECTION, "'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$3",
	       'cdefg', RegExp.$3);

// 'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$4
new TestCase ( SECTION, "'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$4",
	       'def', RegExp.$4);

// 'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$5
new TestCase ( SECTION, "'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$5",
	       'e', RegExp.$5);

// 'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$6
new TestCase ( SECTION, "'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/); RegExp.$6",
	       '', RegExp.$6);

var a_to_z = 'abcdefghijklmnopqrstuvwxyz';
var regexp1 = /(a)b(c)d(e)f(g)h(i)j(k)l(m)n(o)p(q)r(s)t(u)v(w)x(y)z/
  // 'abcdefghijklmnopqrstuvwxyz'.match(/(a)b(c)d(e)f(g)h(i)j(k)l(m)n(o)p(q)r(s)t(u)v(w)x(y)z/); RegExp.$1
  a_to_z.match(regexp1);

new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$1",
	       'a', RegExp.$1);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$2",
	       'c', RegExp.$2);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$3",
	       'e', RegExp.$3);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$4",
	       'g', RegExp.$4);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$5",
	       'i', RegExp.$5);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$6",
	       'k', RegExp.$6);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$7",
	       'm', RegExp.$7);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$8",
	       'o', RegExp.$8);
new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$9",
	       'q', RegExp.$9);
/*
  new TestCase ( SECTION, "'" + a_to_z + "'.match((a)b(c)....(y)z); RegExp.$10",
  's', RegExp.$10);
*/
test();

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

var gTestfile = 'basic-for-in.js';
//-----------------------------------------------------------------------------
var BUGNUMBER     = "346582";
var summary = "Basic support for iterable objects and for-in";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

var iterable = { persistedProp: 17 };

try
{
  // nothing unusual so far -- verify basic properties
  for (var i in iterable)
  {
    if (i != "persistedProp")
      throw "no persistedProp!";
    if (iterable[i] != 17)
      throw "iterable[\"persistedProp\"] == 17";
  }

  var keys = ["foo", "bar", "baz"];
  var vals = [6, 5, 14];

  iterable.__iterator__ =
    function(keysOnly)
    {
      var gen =
      function()
      {
	for (var i = 0; i < keys.length; i++)
	{
	  if (keysOnly)
	    yield keys[i];
	  else
	    yield [keys[i], vals[i]];
	}
      };
      return gen();
    };

  // for in sets keysOnly==true
  var index = 0;
  for (var k in iterable)
  {
    if (k != keys[index])
      throw "for-in iteration failed on keys[\"" + index + "\"]";
    index++;
  }
  if (index != keys.length)
    throw "not everything iterated!  index=" + index +
      ", keys.length=" + keys.length;

  if (iterable.persistedProp != 17)
    throw "iterable.persistedProp not persisted!";
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);

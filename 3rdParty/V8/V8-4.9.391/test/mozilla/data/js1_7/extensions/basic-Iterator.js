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

var gTestfile = 'basic-Iterator.js';
//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "Basic support for accessing iterable objects using Iterator";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function Array_equals(a, b)
{
  if (!(a instanceof Array) || !(b instanceof Array))
    throw new Error("Arguments not both of type Array");
  if (a.length != b.length)
    return false;
  for (var i = 0, sz = a.length; i < sz; i++)
    if (a[i] !== b[i])
      return false;
  return true;
}

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

  /* Test [key, value] Iterator */
  var index = 0;
  var lastSeen = "INITIALVALUE";
  var it = Iterator(iterable);
  try
  {
    while (true)
    {
      var nextVal = it.next();
      if (!Array_equals(nextVal, [keys[index], vals[index]]))
        throw "Iterator(iterable): wrong next result\n" +
	  "  expected: " + [keys[index], vals[index]] + "\n" +
	  "  actual:   " + nextVal;
      lastSeen = keys[index];
      index++;
    }
  }
  catch (e)
  {
    if (lastSeen !== keys[keys.length - 1])
      throw "Iterator(iterable): not everything was iterated!\n" +
	"  last iterated was: " + lastSeen + "\n" +
	"  error: " + e;
    if (e !== StopIteration)
      throw "Iterator(iterable): missing or invalid StopIteration";
  }

  if (iterable.persistedProp != 17)
    throw "iterable.persistedProp not persisted!";

  /* Test [key, value] Iterator, called with an explicit |false| parameter */
  var index = 0;
  lastSeen = "INITIALVALUE";
  it = Iterator(iterable, false);
  try
  {
    while (true)
    {
      var nextVal = it.next();
      if (!Array_equals(nextVal, [keys[index], vals[index]]))
        throw "Iterator(iterable, false): wrong next result\n" +
	  "  expected: " + [keys[index], vals[index]] + "\n" +
	  "  actual:   " + nextVal;
      lastSeen = keys[index];
      index++;
    }
  }
  catch (e)
  {
    if (lastSeen !== keys[keys.length - 1])
      throw "Iterator(iterable, false): not everything was iterated!\n" +
	"  last iterated was: " + lastSeen + "\n" +
	"  error: " + e;
    if (e !== StopIteration)
      throw "Iterator(iterable, false): missing or invalid StopIteration";
  }

  if (iterable.persistedProp != 17)
    throw "iterable.persistedProp not persisted!";

  /* Test key-only Iterator */
  index = 0;
  lastSeen = undefined;
  it = Iterator(iterable, true);
  try
  {
    while (true)
    {
      var nextVal = it.next();
      if (nextVal !== keys[index])
        throw "Iterator(iterable, true): wrong next result\n" +
	  "  expected: " + keys[index] + "\n" +
	  "  actual:   " + nextVal;
      lastSeen = keys[index];
      index++;
    }
  }
  catch (e)
  {
    if (lastSeen !== keys[keys.length - 1])
      throw "Iterator(iterable, true): not everything was iterated!\n" +
	"  last iterated was: " + lastSeen + "\n" +
	"  error: " + e;
    if (e !== StopIteration)
      throw "Iterator(iterable, true): missing or invalid StopIteration";
  }

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

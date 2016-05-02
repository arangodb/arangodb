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
 * Contributor(s): Igor Bukanov
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

var gTestfile = 'regress-349331.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 349331;
var summary = 'generator.close without GeneratorExit';
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
 
  var catch1, catch2, catch3, finally1, finally2, finally3;
  var iter;

  function gen()
  {
    yield 1;
    try {
      try {
        try {
          yield 1;
        } catch (e) {
          catch1 = true;
        } finally {
          finally1 = true;
        }
      } catch (e) {
        catch2 = true;
      } finally {
        finally2 = true;
      }
    } catch (e) {
      catch3 = true;
    } finally {
      finally3 = true;
    }
  }

// test explicit close call
  catch1 = catch2 = catch3 = finally1 = finally2 = finally3 = false;
  iter = gen();
  iter.next();
  iter.next();
  iter.close();

  var passed = !catch1 && !catch2 && !catch3 && finally1 && finally2 &&
    finally3;

  if (!passed) {
    print("Failed!");
    print("catch1=" + catch1 + " catch2=" + catch2 + " catch3=" +
	  catch3);
    print("finally1=" + finally1 + " finally2=" + finally2 +
	  " finally3=" + finally3);
  }

  reportCompare(true, passed, 'test explicit close call');

// test GC-invoked close
  catch1 = catch2 = catch3 = finally1 = finally2 = finally3 = false;
  iter = gen();
  iter.next();
  iter.next();
  iter = null;
  gc();
  gc();

  var passed = !catch1 && !catch2 && !catch3 && finally1 && finally2 &&
    finally3;

  if (!passed) {
    print("Failed!");
    print("catch1=" + catch1 + " catch2=" + catch2 + " catch3=" +
	  catch3);
    print("finally1=" + finally1 + " finally2=" + finally2 +
	  " finally3="+finally3);
  }
  reportCompare(true, passed, 'test GC-invoke close');

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *   Rob Ginda rginda@netscape.com
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

var gTestfile = 'catchguard-003.js';

test();

function test()
{
  enterFunc ("test");

  var EXCEPTION_DATA = "String exception";
  var e = "foo", x = "foo";
  var caught = false;

  printStatus ("Catchguard 'Common Scope' test.");
   
  try
  {   
    throw EXCEPTION_DATA;  
  }
  catch (e if ((x = 1) && false))
  {
    reportCompare('PASS', 'FAIL',
		  "Catch block (e if ((x = 1) && false) should not " +
		  "have executed.");
  }
  catch (e if (x == 1))
  {  
    caught = true;
  }
  catch (e)
  {  
    reportCompare('PASS', 'FAIL',
		  "Same scope should be used across all catchguards.");
  }

  if (!caught)
    reportCompare('PASS', 'FAIL',
		  "Exception was never caught.");
   
  if (e != "foo")
    reportCompare('PASS', 'FAIL',
		  "Exception data modified inside catch() scope should " +
		  "not be visible in the function scope (e ='" +
		  e + "'.)");

  if (x != 1)
    reportCompare('PASS', 'FAIL',
		  "Data modified in 'catchguard expression' should " +
		  "be visible in the function scope (x = '" +
		  x + "'.)");

  reportCompare('PASS', 'PASS', 'Catchguard Common Scope test');

  exitFunc ("test");
}

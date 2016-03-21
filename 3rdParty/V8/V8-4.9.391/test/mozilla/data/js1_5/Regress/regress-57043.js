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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   pschwartau@netscape.com
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

/*
 * Date: 03 December 2000
 *
 *
 * SUMMARY:  This test arose from Bugzilla bug 57043:
 * "Negative integers as object properties: strange behavior!"
 *
 * We check that object properties may be indexed by signed
 * numeric literals, as in assignments like obj[-1] = 'Hello' 
 *
 * NOTE: it should not matter whether we provide the literal with
 * quotes around it or not; e.g. these should be equivalent:
 *
 *                                    obj[-1]  = 'Hello'
 *                                    obj['-1']  = 'Hello'
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-57043.js';
var BUGNUMBER = 57043;
var summary = 'Indexing object properties by signed numerical literals -'
  var statprefix = 'Adding a property to test object with an index of ';
var statsuffix =  ', testing it now -';
var propprefix = 'This is property ';
var obj = new Object();
var status = ''; var actual = ''; var expect = ''; var value = '';


//  various indices to try -
var index = Array(-5000, -507, -3, -2, -1, 0, 1, 2, 3); 


//------------------------------------------------------------------------------------------------- 
test();
//-------------------------------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (j in index) {testProperty(index[j]);}

  exitFunc ('test');
}


function testProperty(i)
{
  status = getStatus(i);

  // try to assign a property using the given index -
  obj[i] = value = (propprefix  +  i);  
 
  // try to read the property back via the index (as number) -
  expect = value;
  actual = obj[i];
  reportCompare(expect, actual, status); 

  // try to read the property back via the index as string -
  expect = value;
  actual = obj[String(i)];
  reportCompare(expect, actual, status);
}


function getStatus(i)
{
  return (statprefix  +  i  +  statsuffix);
}

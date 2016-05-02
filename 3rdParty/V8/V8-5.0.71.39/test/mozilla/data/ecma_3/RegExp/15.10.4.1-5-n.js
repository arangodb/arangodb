/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

var gTestfile = '15.10.4.1-5-n.js';
/*
 *
 * Date: 26 November 2000
 *
 *
 *SUMMARY: Passing a RegExp object to a RegExp() constructor.
 *This test arose from Bugzilla bug 61266. The ECMA3 section is:      
 *
 *  15.10.4.1 new RegExp(pattern, flags)
 *
 *  If pattern is an object R whose [[Class]] property is "RegExp" and
 *  flags is undefined, then let P be the pattern used to construct R
 *  and let F be the flags used to construct R. If pattern is an object R
 *  whose [[Class]] property is "RegExp" and flags is not undefined,
 *  then throw a TypeError exception. Otherwise, let P be the empty string 
 *  if pattern is undefined and ToString(pattern) otherwise, and let F be
 *  the empty string if flags is undefined and ToString(flags) otherwise.
 *
 *
 *The current test will check the second scenario outlined above:
 *
 *   "pattern" is itself a RegExp object R
 *   "flags" is NOT undefined
 *
 * This should throw an exception ... we test for this.
 *
 */

//-------------------------------------------------------------------------------------------------
var BUGNUMBER = '61266';
var summary = 'Negative test: Passing (RegExp object, flag) to RegExp() constructor';
var statprefix = 'Passing RegExp object on pattern ';
var statsuffix =  '; passing flag ';
var cnFAILURE = 'Expected an exception to be thrown, but none was -';
var singlequote = "'";
var i = -1; var j = -1; var s = ''; var f = '';
var obj1 = {}; var obj2 = {};
var patterns = new Array();
var flags = new Array(); 


// various regular expressions to try -
patterns[0] = '';
patterns[1] = 'abc';
patterns[2] = '(.*)(3-1)\s\w';
patterns[3] = '(.*)(...)\\s\\w';
patterns[4] = '[^A-Za-z0-9_]';
patterns[5] = '[^\f\n\r\t\v](123.5)([4 - 8]$)';

// various flags to try -
flags[0] = 'i';
flags[1] = 'g';
flags[2] = 'm';


DESCRIPTION = "Negative test: Passing (RegExp object, flag) to RegExp() constructor"
  EXPECTED = "error";


//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (i in patterns)
  {
    s = patterns[i];

    for (j in flags)
    {
      f = flags[j];
      printStatus(getStatus(s, f));
      obj1 = new RegExp(s, f);  
      obj2 = new RegExp(obj1, f);   // this should cause an exception

      // WE SHOULD NEVER REACH THIS POINT -
      reportCompare('PASS', 'FAIL', cnFAILURE);
    }
  }

  exitFunc ('test');
}


function getStatus(regexp, flag)
{
  return (statprefix  +  quote(regexp) +  statsuffix  +   flag);
}


function quote(text)
{
  return (singlequote  +  text  + singlequote);
}

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
 *   pschwartau@netscape.com, zack-weg@gmx.de
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
 * Date: 26 November 2000
 *
 *
 * SUMMARY:  This test arose from Bugzilla bug 57631:
 * "RegExp with invalid pattern or invalid flag causes segfault"
 *
 * Either error should throw an exception of type SyntaxError,
 * and we check to see that it does...
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-57631.js';
var BUGNUMBER = '57631';
var summary = 'Testing new RegExp(pattern,flag) with illegal pattern or flag';
var statprefix = 'Testing for error creating illegal RegExp object on pattern ';
var statsuffix =  'and flag ';
var cnSUCCESS = 'SyntaxError';
var cnFAILURE = 'not a SyntaxError';
var singlequote = "'";
var i = -1; var j = -1; var s = ''; var f = '';
var obj = {};
var status = ''; var actual = ''; var expect = ''; var msg = '';
var legalpatterns = new Array(); var illegalpatterns = new Array();
var legalflags = new Array();  var illegalflags = new Array();


// valid regular expressions to try -
legalpatterns[0] = '';
legalpatterns[1] = 'abc';
legalpatterns[2] = '(.*)(3-1)\s\w';
legalpatterns[3] = '(.*)(...)\\s\\w';
legalpatterns[4] = '[^A-Za-z0-9_]';
legalpatterns[5] = '[^\f\n\r\t\v](123.5)([4 - 8]$)';

// invalid regular expressions to try -
illegalpatterns[0] = '(?)';
illegalpatterns[1] = '(a';
illegalpatterns[2] = '( ]';
//illegalpatterns[3] = '\d{1,s}';

// valid flags to try -
legalflags[0] = 'i';
legalflags[1] = 'g';
legalflags[2] = 'm';
legalflags[3] = undefined;

// invalid flags to try -
illegalflags[0] = 'a';
illegalflags[1] = 123;
illegalflags[2] = new RegExp();



//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  testIllegalRegExps(legalpatterns, illegalflags);
  testIllegalRegExps(illegalpatterns, legalflags);
  testIllegalRegExps(illegalpatterns, illegalflags);

  exitFunc ('test');
}


// This function will only be called where all the patterns are illegal, or all the flags
function testIllegalRegExps(patterns, flags)
{
  for (i in patterns)
  {
    s = patterns[i];
 
    for (j in flags)
    {
      f = flags[j];
      status = getStatus(s, f);
      actual = cnFAILURE;
      expect = cnSUCCESS;
 
      try
      {
	// This should cause an exception if either s or f is illegal -        
	eval('obj = new RegExp(s, f);'); 
      } 
      catch(e)
      {
	// We expect to get a SyntaxError - test for this:
	if (e instanceof SyntaxError)
	  actual = cnSUCCESS;
      }
       
      reportCompare(expect, actual, status);
    }
  }
}


function getStatus(regexp, flag)
{
  return (statprefix  +  quote(regexp) +  statsuffix  +   quote(flag));
}


function quote(text)
{
  return (singlequote  +  text  + singlequote);
}

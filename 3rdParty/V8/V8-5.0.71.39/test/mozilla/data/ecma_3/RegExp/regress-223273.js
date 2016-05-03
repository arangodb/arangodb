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
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
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
 *
 * Date:    23 October 2003
 * SUMMARY: Unescaped, unbalanced parens in a regexp should cause SyntaxError.
 *
 * The same would also be true for unescaped, unbalanced brackets or braces
 * if we followed the ECMA-262 Ed. 3 spec on this. But it was decided for
 * backward compatibility reasons to follow Perl 5, which permits
 *
 * 1. an unescaped, unbalanced right bracket ]
 * 2. an unescaped, unbalanced left brace    {
 * 3. an unescaped, unbalanced right brace   }
 *
 * If any of these should occur, Perl treats each as a literal
 * character.  Therefore we permit all three of these cases, even
 * though not ECMA-compliant.  Note Perl errors on an unescaped,
 * unbalanced left bracket; so will we.
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=223273
 *
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-223273.js';
var UBound = 0;
var BUGNUMBER = 223273;
var summary = 'Unescaped, unbalanced parens in regexp should be a SyntaxError';
var TEST_PASSED = 'SyntaxError';
var TEST_FAILED = 'Generated an error, but NOT a SyntaxError!';
var TEST_FAILED_BADLY = 'Did not generate ANY error!!!';
var CHECK_PASSED = 'Should not generate an error';
var CHECK_FAILED = 'Generated an error!';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * All the following contain unescaped, unbalanced parens and
 * should generate SyntaxErrors. That's what we're testing for.
 *
 * To allow the test to compile and run, we have to hide the errors
 * inside eval strings, and check they are caught at run-time.
 *
 * Inside such strings, remember to escape any escape character!
 */
status = inSection(1);
testThis(' /(/ ');

status = inSection(2);
testThis(' /)/ ');

status = inSection(3);
testThis(' /(abc\\)def(g/ ');

status = inSection(4);
testThis(' /\\(abc)def)g/ ');


/*
 * These regexp patterns are correct and should not generate
 * any errors. Note we use checkThis() instead of testThis().
 */
status = inSection(5);
checkThis(' /\\(/ ');

status = inSection(6);
checkThis(' /\\)/ ');

status = inSection(7);
checkThis(' /(abc)def\\(g/ ');

status = inSection(8);
checkThis(' /(abc\\)def)g/ ');

status = inSection(9);
checkThis(' /(abc(\\))def)g/ ');

status = inSection(10);
checkThis(' /(abc([x\\)yz]+)def)g/ ');



/*
 * Unescaped, unbalanced left brackets should be a SyntaxError
 */
status = inSection(11);
testThis(' /[/ ');

status = inSection(12);
testThis(' /[abc\\]def[g/ ');


/*
 * We permit unescaped, unbalanced right brackets, as does Perl.
 * No error should result, even though this is not ECMA-compliant.
 * Note we use checkThis() instead of testThis().
 */
status = inSection(13);
checkThis(' /]/ ');

status = inSection(14);
checkThis(' /\\[abc]def]g/ ');


/*
 * These regexp patterns are correct and should not generate
 * any errors. Note we use checkThis() instead of testThis().
 */
status = inSection(15);
checkThis(' /\\[/ ');

status = inSection(16);
checkThis(' /\\]/ ');

status = inSection(17);
checkThis(' /[abc]def\\[g/ ');

status = inSection(18);
checkThis(' /[abc\\]def]g/ ');

status = inSection(19);
checkThis(' /(abc[\\]]def)g/ ');

status = inSection(20);
checkThis(' /[abc(x\\]yz+)def]g/ ');



/*
 * Run some tests for unbalanced braces. We again follow Perl, and
 * thus permit unescaped unbalanced braces - both left and right,
 * even though this is not ECMA-compliant.
 *
 * Note we use checkThis() instead of testThis().
 */
status = inSection(21);
checkThis(' /abc{def/ ');

status = inSection(22);
checkThis(' /abc}def/ ');

status = inSection(23);
checkThis(' /a{2}bc{def/ ');

status = inSection(24);
checkThis(' /a}b{3}c}def/ ');


/*
 * These regexp patterns are correct and should not generate
 * any errors. Note we use checkThis() instead of testThis().
 */
status = inSection(25);
checkThis(' /abc\\{def/ ');

status = inSection(26);
checkThis(' /abc\\}def/ ');

status = inSection(27);
checkThis(' /a{2}bc\\{def/ ');

status = inSection(28);
checkThis(' /a\\}b{3}c\\}def/ ');




//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------




/*
 * Invalid syntax should generate a SyntaxError
 */
function testThis(sInvalidSyntax)
{
  expect = TEST_PASSED;
  actual = TEST_FAILED_BADLY;

  try
  {
    eval(sInvalidSyntax);
  }
  catch(e)
  {
    if (e instanceof SyntaxError)
      actual = TEST_PASSED;
    else
      actual = TEST_FAILED;
  }

  statusitems[UBound] = status;
  expectedvalues[UBound] = expect;
  actualvalues[UBound] = actual;
  UBound++;
}


/*
 * Valid syntax shouldn't generate any errors
 */
function checkThis(sValidSyntax)
{
  expect = CHECK_PASSED;
  actual = CHECK_PASSED;

  try
  {
    eval(sValidSyntax);
  }
  catch(e)
  {
    actual = CHECK_FAILED;
  }

  statusitems[UBound] = status;
  expectedvalues[UBound] = expect;
  actualvalues[UBound] = actual;
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}

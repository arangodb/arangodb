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
 *   scole@planetweb.com, pschwartau@netscape.com
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

//-----------------------------------------------------------------------------
var gTestfile = 'regress-188206-02.js';
var UBound = 0;
var BUGNUMBER = 188206;
var summary = 'Invalid use of regexp quantifiers should generate SyntaxErrors';
var CHECK_PASSED = 'Should not generate an error';
var CHECK_FAILED = 'Generated an error!';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * Misusing the {DecmalDigits} quantifier - according to ECMA,
 * but not according to Perl.
 *
 * ECMA-262 Edition 3 prohibits the use of unescaped braces in
 * regexp patterns, unless they form part of a quantifier.
 *
 * Hovever, Perl does not prohibit this. If not used as part
 * of a quantifer, Perl treats braces literally.
 *
 * We decided to follow Perl on this for backward compatibility.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=190685.
 *
 * Therefore NONE of the following ECMA violations should generate
 * a SyntaxError. Note we use checkThis() instead of testThis().
 */
status = inSection(13);
checkThis(' /a*{/ ');

status = inSection(14);
checkThis(' /a{}/ ');

status = inSection(15);
checkThis(' /{a/ ');

status = inSection(16);
checkThis(' /}a/ ');

status = inSection(17);
checkThis(' /x{abc}/ ');

status = inSection(18);
checkThis(' /{{0}/ ');

status = inSection(19);
checkThis(' /{{1}/ ');

status = inSection(20);
checkThis(' /x{{0}/ ');

status = inSection(21);
checkThis(' /x{{1}/ ');

status = inSection(22);
checkThis(' /x{{0}}/ ');

status = inSection(23);
checkThis(' /x{{1}}/ ');

status = inSection(24);
checkThis(' /x{{0}}/ ');

status = inSection(25);
checkThis(' /x{{1}}/ ');

status = inSection(26);
checkThis(' /x{{0}}/ ');

status = inSection(27);
checkThis(' /x{{1}}/ ');


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



/*
 * Allowed syntax shouldn't generate any errors
 */
function checkThis(sAllowedSyntax)
{
  expect = CHECK_PASSED;
  actual = CHECK_PASSED;

  try
  {
    eval(sAllowedSyntax);
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

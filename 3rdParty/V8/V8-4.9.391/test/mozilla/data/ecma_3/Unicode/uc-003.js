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

var gTestfile = 'uc-003.js';

test();

function test()
{
  enterFunc ("test");

  var \u0041 = 5;
  var A\u03B2 = 15;
  var c\u0061se = 25;

  printStatus ("Escapes in identifiers test.");
  printBugNumber (23608);
  printBugNumber (23607);

  reportCompare (5, eval("\u0041"),
		 "Escaped ASCII Identifier test.");
  reportCompare (6, eval("++\u0041"),
		 "Escaped ASCII Identifier test");
  reportCompare (15, eval("A\u03B2"),
		 "Escaped non-ASCII Identifier test");
  reportCompare (16, eval("++A\u03B2"),
		 "Escaped non-ASCII Identifier test");
  reportCompare (25, eval("c\\u00" + "61se"),
		 "Escaped keyword Identifier test");
  reportCompare (26, eval("++c\\u00" + "61se"),
		 "Escaped keyword Identifier test");
   
  exitFunc ("test");
}

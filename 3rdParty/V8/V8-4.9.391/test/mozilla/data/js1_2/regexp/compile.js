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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

gTestfile = 'compile.js';

/**
   Filename:     compile.js
   Description:  'Tests regular expressions method compile'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: compile';

writeHeaderToLog('Executing script: compile.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var regularExpression = new RegExp();

regularExpression.compile("[0-9]{3}x[0-9]{4}","i");

new TestCase ( SECTION,
	       "(compile '[0-9]{3}x[0-9]{4}','i')",
	       String(["456X7890"]), String('234X456X7890'.match(regularExpression)));

new TestCase ( SECTION,
	       "source of (compile '[0-9]{3}x[0-9]{4}','i')",
	       "[0-9]{3}x[0-9]{4}", regularExpression.source);

new TestCase ( SECTION,
	       "global of (compile '[0-9]{3}x[0-9]{4}','i')",
	       false, regularExpression.global);

new TestCase ( SECTION,
	       "ignoreCase of (compile '[0-9]{3}x[0-9]{4}','i')",
	       true, regularExpression.ignoreCase);

regularExpression.compile("[0-9]{3}X[0-9]{3}","g");

new TestCase ( SECTION,
	       "(compile '[0-9]{3}X[0-9]{3}','g')",
	       String(["234X456"]), String('234X456X7890'.match(regularExpression)));

new TestCase ( SECTION,
	       "source of (compile '[0-9]{3}X[0-9]{3}','g')",
	       "[0-9]{3}X[0-9]{3}", regularExpression.source);

new TestCase ( SECTION,
	       "global of (compile '[0-9]{3}X[0-9]{3}','g')",
	       true, regularExpression.global);

new TestCase ( SECTION,
	       "ignoreCase of (compile '[0-9]{3}X[0-9]{3}','g')",
	       false, regularExpression.ignoreCase);


test();

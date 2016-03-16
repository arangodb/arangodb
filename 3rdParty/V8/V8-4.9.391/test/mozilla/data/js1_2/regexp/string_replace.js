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

gTestfile = 'string_replace.js';

/**
   Filename:     string_replace.js
   Description:  'Tests the replace method on Strings using regular expressions'

   Author:       Nick Lerissa
   Date:         March 11, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'String: replace';

writeHeaderToLog('Executing script: string_replace.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'adddb'.replace(/ddd/,"XX")
new TestCase ( SECTION, "'adddb'.replace(/ddd/,'XX')",
	       "aXXb", 'adddb'.replace(/ddd/,'XX'));

// 'adddb'.replace(/eee/,"XX")
new TestCase ( SECTION, "'adddb'.replace(/eee/,'XX')",
	       'adddb', 'adddb'.replace(/eee/,'XX'));

// '34 56 78b 12'.replace(new RegExp('[0-9]+b'),'**')
new TestCase ( SECTION, "'34 56 78b 12'.replace(new RegExp('[0-9]+b'),'**')",
	       "34 56 ** 12", '34 56 78b 12'.replace(new RegExp('[0-9]+b'),'**'));

// '34 56 78b 12'.replace(new RegExp('[0-9]+c'),'XX')
new TestCase ( SECTION, "'34 56 78b 12'.replace(new RegExp('[0-9]+c'),'XX')",
	       "34 56 78b 12", '34 56 78b 12'.replace(new RegExp('[0-9]+c'),'XX'));

// 'original'.replace(new RegExp(),'XX')
new TestCase ( SECTION, "'original'.replace(new RegExp(),'XX')",
	       "XXoriginal", 'original'.replace(new RegExp(),'XX'));

// 'qwe ert x\t\n 345654AB'.replace(new RegExp('x\s*\d+(..)$'),'****')
new TestCase ( SECTION, "'qwe ert x\t\n 345654AB'.replace(new RegExp('x\\s*\\d+(..)$'),'****')",
	       "qwe ert ****", 'qwe ert x\t\n 345654AB'.replace(new RegExp('x\\s*\\d+(..)$'),'****'));


test();

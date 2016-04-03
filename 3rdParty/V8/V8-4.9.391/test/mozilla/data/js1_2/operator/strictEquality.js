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

gTestfile = 'strictEquality.js';

/**
   Filename:     strictEquality.js
   Description:  'This tests the operator ==='

   Author:       Nick Lerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'operator "==="';

writeHeaderToLog('Executing script: strictEquality.js');
writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, "('8' === 8)                              ",
	      false,  ('8' === 8));

new TestCase( SECTION, "(8 === 8)                                ",
	      true,   (8 === 8));

new TestCase( SECTION, "(8 === true)                             ",
	      false,  (8 === true));

new TestCase( SECTION, "(new String('') === new String(''))      ",
	      false,  (new String('') === new String('')));

new TestCase( SECTION, "(new Boolean(true) === new Boolean(true))",
	      false,  (new Boolean(true) === new Boolean(true)));

var anObject = { one:1 , two:2 };

new TestCase( SECTION, "(anObject === anObject)                  ",
	      true,  (anObject === anObject));

new TestCase( SECTION, "(anObject === { one:1 , two:2 })         ",
	      false,  (anObject === { one:1 , two:2 }));

new TestCase( SECTION, "({ one:1 , two:2 } === anObject)         ",
	      false,  ({ one:1 , two:2 } === anObject));

new TestCase( SECTION, "(null === null)                          ",
	      true,  (null === null));

new TestCase( SECTION, "(null === 0)                             ",
	      false,  (null === 0));

new TestCase( SECTION, "(true === !false)                        ",
	      true,  (true === !false));

test();


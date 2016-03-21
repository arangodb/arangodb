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

gTestfile = 'RegExp_object.js';

/**
   Filename:     RegExp_object.js
   Description:  'Tests regular expressions creating RexExp Objects'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: object';

writeHeaderToLog('Executing script: RegExp_object.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var SSN_pattern = new RegExp("\\d{3}-\\d{2}-\\d{4}");

// testing SSN pattern
new TestCase ( SECTION, "'Test SSN is 123-34-4567'.match(SSN_pattern))",
	       String(["123-34-4567"]), String('Test SSN is 123-34-4567'.match(SSN_pattern)));

// testing SSN pattern
new TestCase ( SECTION, "'Test SSN is 123-34-4567'.match(SSN_pattern))",
	       String(["123-34-4567"]), String('Test SSN is 123-34-4567'.match(SSN_pattern)));

var PHONE_pattern = new RegExp("\\(?(\\d{3})\\)?-?(\\d{3})-(\\d{4})");
// testing PHONE pattern
new TestCase ( SECTION, "'Our phone number is (408)345-2345.'.match(PHONE_pattern))",
	       String(["(408)345-2345","408","345","2345"]), String('Our phone number is (408)345-2345.'.match(PHONE_pattern)));

// testing PHONE pattern
new TestCase ( SECTION, "'The phone number is 408-345-2345!'.match(PHONE_pattern))",
	       String(["408-345-2345","408","345","2345"]), String('The phone number is 408-345-2345!'.match(PHONE_pattern)));

// testing PHONE pattern
new TestCase ( SECTION, "String(PHONE_pattern.toString())",
	       "/\\(?(\\d{3})\\)?-?(\\d{3})-(\\d{4})/", String(PHONE_pattern.toString()));

// testing conversion to String
new TestCase ( SECTION, "PHONE_pattern + ' is the string'",
	       "/\\(?(\\d{3})\\)?-?(\\d{3})-(\\d{4})/ is the string",PHONE_pattern + ' is the string');

// testing conversion to int
new TestCase ( SECTION, "SSN_pattern - 8",
	       NaN,SSN_pattern - 8);

var testPattern = new RegExp("(\\d+)45(\\d+)90");

test();
